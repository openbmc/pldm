#include "cper.hpp"

#include "libcper/Cper.h"

#include "common/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cstring>
#include <ranges>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace oem_ampere
{

// Returns one if two EFI GUIDs are equal, zero otherwise.
int guid_equal(EFI_GUID* a, EFI_GUID* b)
{
    // Check top base 3 components.
    if (a->Data1 != b->Data1 || a->Data2 != b->Data2 || a->Data3 != b->Data3)
    {
        return 0;
    }

    // Check Data4 array for equality.
    for (int i = 0; i < 8; i++)
    {
        if (a->Data4[i] != b->Data4[i])
        {
            return 0;
        }
    }

    return 1;
}

static void decodeSecAmpere(void* section, EFI_AMPERE_ERROR_DATA* ampSpecHdr)
{
    std::memcpy(ampSpecHdr, section, sizeof(EFI_AMPERE_ERROR_DATA));
}

static void decodeSecArm(void* section, EFI_AMPERE_ERROR_DATA* ampSpecHdr)
{
    int len;
    EFI_ARM_ERROR_RECORD* proc;
    EFI_ARM_ERROR_INFORMATION_ENTRY* errInfo;
    EFI_ARM_CONTEXT_INFORMATION_HEADER* ctxInfo;

    proc = reinterpret_cast<EFI_ARM_ERROR_RECORD*>(section);
    errInfo = reinterpret_cast<EFI_ARM_ERROR_INFORMATION_ENTRY*>(proc + 1);

    len = proc->SectionLength -
          (sizeof(EFI_ARM_ERROR_RECORD) +
           proc->ErrInfoNum * (sizeof(EFI_ARM_ERROR_INFORMATION_ENTRY)));
    if (len < 0)
    {
        lg2::error("Section length is too small, section_length {LENGTH}",
                   "LENGTH", static_cast<int>(proc->SectionLength));
        return;
    }

    ctxInfo = reinterpret_cast<EFI_ARM_CONTEXT_INFORMATION_HEADER*>(
        errInfo + proc->ErrInfoNum);

    for ([[maybe_unused]] const auto& i :
         std::views::iota(0, static_cast<int>(proc->ContextInfoNum)))
    {
        int size = sizeof(EFI_ARM_CONTEXT_INFORMATION_HEADER) +
                   ctxInfo->RegisterArraySize;
        len -= size;
        ctxInfo = reinterpret_cast<EFI_ARM_CONTEXT_INFORMATION_HEADER*>(
            (long)ctxInfo + size);
    }

    if (len > 0)
    {
        /* Get Ampere Specific header data */
        std::memcpy(ampSpecHdr, (void*)ctxInfo, sizeof(EFI_AMPERE_ERROR_DATA));
    }
}

static void decodeSecPlatformMemory(void* section,
                                    EFI_AMPERE_ERROR_DATA* ampSpecHdr)
{
    EFI_PLATFORM_MEMORY_ERROR_DATA* mem =
        reinterpret_cast<EFI_PLATFORM_MEMORY_ERROR_DATA*>(section);
    if (mem->ErrorType == MEM_ERROR_TYPE_PARITY)
    {
        /* IP Type from bit 0 to 11 of TypeId */
        ampSpecHdr->TypeId = (ampSpecHdr->TypeId & 0xf800) + ERROR_TYPE_ID_MCU;
        ampSpecHdr->SubtypeId = SUBTYPE_ID_PARITY;
    }
}

static void decodeSecPcie(void* section, EFI_AMPERE_ERROR_DATA* ampSpecHdr)
{
    EFI_PCIE_ERROR_DATA* pcieErr =
        reinterpret_cast<EFI_PCIE_ERROR_DATA*>(section);
    if (pcieErr->ValidFields & CPER_PCIE_VALID_PORT_TYPE)
    {
        if (pcieErr->PortType == CPER_PCIE_PORT_TYPE_ROOT_PORT)
        {
            ampSpecHdr->SubtypeId = ERROR_SUBTYPE_PCIE_AER_ROOT_PORT;
        }
        else
        {
            ampSpecHdr->SubtypeId = ERROR_SUBTYPE_PCIE_AER_DEVICE;
        }
    }
}

static void decodeCperSection(const uint8_t* data, long basePos,
                              EFI_AMPERE_ERROR_DATA* ampSpecHdr,
                              EFI_ERROR_SECTION_DESCRIPTOR* secDesc)
{
    long pos = basePos + secDesc->SectionOffset;
    char* section = new char[secDesc->SectionLength];
    std::memcpy(section, &data[pos], secDesc->SectionLength);
    pos += secDesc->SectionLength;
    EFI_GUID* ptr = reinterpret_cast<EFI_GUID*>(&secDesc->SectionType);
    if (guid_equal(ptr, &gEfiAmpereErrorSectionGuid))
    {
        lg2::info("RAS Section Type : Ampere Specific");
        decodeSecAmpere(section, ampSpecHdr);
    }
    else if (guid_equal(ptr, &gEfiArmProcessorErrorSectionGuid))
    {
        lg2::info("RAS Section Type : ARM");
        decodeSecArm(section, ampSpecHdr);
    }
    else if (guid_equal(ptr, &gEfiPlatformMemoryErrorSectionGuid))
    {
        lg2::info("RAS Section Type : Memory");
        decodeSecPlatformMemory(section, ampSpecHdr);
    }
    else if (guid_equal(ptr, &gEfiPcieErrorSectionGuid))
    {
        lg2::info("RAS Section Type : PCIE");
        decodeSecPcie(section, ampSpecHdr);
    }
    else
    {
        lg2::error("Section Type is not supported");
    }

    delete[] section;
}

void decodeCperRecord(const uint8_t* data, size_t /* eventDataSize */,
                      EFI_AMPERE_ERROR_DATA* ampSpecHdr)
{
    EFI_COMMON_ERROR_RECORD_HEADER cperHeader;
    long pos = sizeof(CommonEventData);
    long basePos = sizeof(CommonEventData);

    std::memcpy(&cperHeader, &data[pos],
                sizeof(EFI_COMMON_ERROR_RECORD_HEADER));
    pos += sizeof(EFI_COMMON_ERROR_RECORD_HEADER);

    EFI_ERROR_SECTION_DESCRIPTOR* secDesc =
        new EFI_ERROR_SECTION_DESCRIPTOR[cperHeader.SectionCount];
    for ([[maybe_unused]] const auto& i :
         std::views::iota(0, static_cast<int>(cperHeader.SectionCount)))
    {
        std::memcpy(&secDesc[i], &data[pos],
                    sizeof(EFI_ERROR_SECTION_DESCRIPTOR));
        pos += sizeof(EFI_ERROR_SECTION_DESCRIPTOR);
    }

    for ([[maybe_unused]] const auto& i :
         std::views::iota(0, static_cast<int>(cperHeader.SectionCount)))
    {
        decodeCperSection(data, basePos, ampSpecHdr, &secDesc[i]);
    }

    delete[] secDesc;
}

void addCperSELLog(pldm_tid_t tid, uint16_t eventID, EFI_AMPERE_ERROR_DATA* p)
{
    std::vector<uint8_t> evtData;
    std::string message = "PLDM RAS SEL Event";
    uint8_t recordType;
    uint8_t evtData1, evtData2, evtData3, evtData4, evtData5, evtData6;
    uint8_t socket;

    /*
     * OEM IPMI SEL Recode Format for RAS event:
     * evtData1:
     *    Bit [7:4]: eventClass
     *        0xF: oemEvent for RAS
     *    Bit [3:1]: Reserved
     *    Bit 0: SocketID
     *        0x0: Socket 0 0x1: Socket 1
     * evtData2:
     *    Event ID, indicates RAS PLDM sensor ID.
     * evtData3:
     *     Error Type ID high byte - Bit [15:8]
     * evtData4:
     *     Error Type ID low byte - Bit [7:0]
     * evtData5:
     *     Error Sub Type ID high byte
     * evtData6:
     *     Error Sub Type ID low byte
     */
    socket = (tid == 1) ? 0 : 1;
    recordType = 0xD0;
    evtData1 = SENSOR_TYPE_OEM | socket;
    evtData2 = eventID;
    evtData3 = p->TypeId >> 8;
    evtData4 = p->TypeId;
    evtData5 = p->SubtypeId >> 8;
    evtData6 = p->SubtypeId;
    /*
     * OEM data bytes
     *    Ampere IANA: 3 bytes [0x3a 0xcd 0x00]
     *    event data: 9 bytes [evtData1 evtData2 evtData3
     *                         evtData4 evtData5 evtData6
     *                         0x00     0x00     0x00 ]
     *    sel type: 1 byte [0xC0]
     */
    evtData.push_back(0x3a);
    evtData.push_back(0xcd);
    evtData.push_back(0);
    evtData.push_back(evtData1);
    evtData.push_back(evtData2);
    evtData.push_back(evtData3);
    evtData.push_back(evtData4);
    evtData.push_back(evtData5);
    evtData.push_back(evtData6);
    evtData.push_back(0);
    evtData.push_back(0);
    evtData.push_back(0);
    auto& bus = pldm::utils::DBusHandler::getBus();
    try
    {
        auto method =
            bus.new_method_call(logBusName, logPath, logIntf, "IpmiSelAddOem");
        method.append(message, evtData, recordType);

        auto selReply = bus.call(method);
        if (selReply.is_method_error())
        {
            lg2::error("addCperSELLog: add SEL log error");
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("call addCperSELLog error - {ERROR}", "ERROR", e);
    }
}

} // namespace oem_ampere
} // namespace pldm
