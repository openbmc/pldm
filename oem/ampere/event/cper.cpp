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
int guid_equal(const EFI_GUID* a, const EFI_GUID* b)
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

static bool decodeSecAmpere(const uint8_t* section, size_t sectionSize,
                            EFI_AMPERE_ERROR_DATA* ampSpecHdr)
{
    if (sectionSize < sizeof(EFI_AMPERE_ERROR_DATA))
    {
        return false;
    }
    std::memcpy(ampSpecHdr, section, sizeof(EFI_AMPERE_ERROR_DATA));
    return true;
}

static bool decodeSecArm(const uint8_t* section, size_t sectionSize,
                         EFI_AMPERE_ERROR_DATA* ampSpecHdr)
{
    if (sectionSize < sizeof(EFI_ARM_ERROR_RECORD))
    {
        return false;
    }

    EFI_ARM_ERROR_RECORD proc{};
    std::memcpy(&proc, section, sizeof(proc));

    if (proc.SectionLength > sectionSize ||
        proc.SectionLength < sizeof(EFI_ARM_ERROR_RECORD) ||
        proc.ErrInfoNum >
            (proc.SectionLength - sizeof(EFI_ARM_ERROR_RECORD)) /
                sizeof(EFI_ARM_ERROR_INFORMATION_ENTRY))
    {
        lg2::error("Section length is too small, section_length {LENGTH}",
                   "LENGTH", static_cast<int>(proc.SectionLength));
        return false;
    }

    size_t offset = sizeof(EFI_ARM_ERROR_RECORD) +
                    proc.ErrInfoNum *
                        sizeof(EFI_ARM_ERROR_INFORMATION_ENTRY);
    size_t remaining = proc.SectionLength - offset;

    for (size_t i = 0; i < proc.ContextInfoNum; i++)
    {
        if (remaining < sizeof(EFI_ARM_CONTEXT_INFORMATION_HEADER))
        {
            return false;
        }

        EFI_ARM_CONTEXT_INFORMATION_HEADER contextInfo{};
        std::memcpy(&contextInfo, section + offset, sizeof(contextInfo));
        size_t contextSize = sizeof(EFI_ARM_CONTEXT_INFORMATION_HEADER) +
                             contextInfo.RegisterArraySize;
        if (contextSize < sizeof(EFI_ARM_CONTEXT_INFORMATION_HEADER) ||
            contextSize > remaining)
        {
            return false;
        }

        offset += contextSize;
        remaining -= contextSize;
    }

    if (remaining < sizeof(EFI_AMPERE_ERROR_DATA))
    {
        return false;
    }

    std::memcpy(ampSpecHdr, section + offset, sizeof(EFI_AMPERE_ERROR_DATA));
    return true;
}

static bool decodeSecPlatformMemory(const uint8_t* section, size_t sectionSize,
                                    EFI_AMPERE_ERROR_DATA* ampSpecHdr)
{
    if (sectionSize < sizeof(EFI_PLATFORM_MEMORY_ERROR_DATA))
    {
        return false;
    }
    EFI_PLATFORM_MEMORY_ERROR_DATA mem{};
    std::memcpy(&mem, section, sizeof(mem));
    if (mem.ErrorType == MEM_ERROR_TYPE_PARITY)
    {
        /* IP Type from bit 0 to 11 of TypeId */
        ampSpecHdr->TypeId = (ampSpecHdr->TypeId & 0xf800) + ERROR_TYPE_ID_MCU;
        ampSpecHdr->SubtypeId = SUBTYPE_ID_PARITY;
    }
    return true;
}

static bool decodeSecPcie(const uint8_t* section, size_t sectionSize,
                          EFI_AMPERE_ERROR_DATA* ampSpecHdr)
{
    if (sectionSize < sizeof(EFI_PCIE_ERROR_DATA))
    {
        return false;
    }
    EFI_PCIE_ERROR_DATA pcieErr{};
    std::memcpy(&pcieErr, section, sizeof(pcieErr));
    if (pcieErr.ValidFields & CPER_PCIE_VALID_PORT_TYPE)
    {
        if (pcieErr.PortType == CPER_PCIE_PORT_TYPE_ROOT_PORT)
        {
            ampSpecHdr->SubtypeId = ERROR_SUBTYPE_PCIE_AER_ROOT_PORT;
        }
        else
        {
            ampSpecHdr->SubtypeId = ERROR_SUBTYPE_PCIE_AER_DEVICE;
        }
    }
    return true;
}

static bool decodeCperSection(const uint8_t* data, size_t eventDataSize,
                              EFI_AMPERE_ERROR_DATA* ampSpecHdr,
                              const EFI_ERROR_SECTION_DESCRIPTOR* secDesc)
{
    constexpr size_t basePos = sizeof(CommonEventData);
    if (eventDataSize < basePos || secDesc->SectionOffset >
                                    eventDataSize - basePos ||
        secDesc->SectionLength >
            eventDataSize - basePos - secDesc->SectionOffset)
    {
        return false;
    }

    const uint8_t* section = data + basePos + secDesc->SectionOffset;
    size_t sectionSize = secDesc->SectionLength;
    const EFI_GUID* ptr = reinterpret_cast<const EFI_GUID*>(&secDesc->SectionType);
    bool decoded = true;
    if (guid_equal(ptr, &gEfiAmpereErrorSectionGuid))
    {
        lg2::info("RAS Section Type : Ampere Specific");
        decoded = decodeSecAmpere(section, sectionSize, ampSpecHdr);
    }
    else if (guid_equal(ptr, &gEfiArmProcessorErrorSectionGuid))
    {
        lg2::info("RAS Section Type : ARM");
        decoded = decodeSecArm(section, sectionSize, ampSpecHdr);
    }
    else if (guid_equal(ptr, &gEfiPlatformMemoryErrorSectionGuid))
    {
        lg2::info("RAS Section Type : Memory");
        decoded = decodeSecPlatformMemory(section, sectionSize, ampSpecHdr);
    }
    else if (guid_equal(ptr, &gEfiPcieErrorSectionGuid))
    {
        lg2::info("RAS Section Type : PCIE");
        decoded = decodeSecPcie(section, sectionSize, ampSpecHdr);
    }
    else
    {
        lg2::error("Section Type is not supported");
    }

    return decoded;
}

bool decodeCperRecord(const uint8_t* data, size_t eventDataSize,
                      EFI_AMPERE_ERROR_DATA* ampSpecHdr)
{
    constexpr size_t basePos = sizeof(CommonEventData);
    if (data == nullptr || ampSpecHdr == nullptr ||
        eventDataSize < basePos + sizeof(EFI_COMMON_ERROR_RECORD_HEADER))
    {
        return false;
    }

    EFI_COMMON_ERROR_RECORD_HEADER cperHeader;
    size_t pos = basePos;

    std::memcpy(&cperHeader, &data[pos],
                sizeof(EFI_COMMON_ERROR_RECORD_HEADER));
    pos += sizeof(EFI_COMMON_ERROR_RECORD_HEADER);

    const size_t remainingEventData = eventDataSize - pos;
    const size_t sectionCount = cperHeader.SectionCount;
    if (sectionCount >
        remainingEventData / sizeof(EFI_ERROR_SECTION_DESCRIPTOR))
    {
        return false;
    }

    for (size_t i = 0; i < sectionCount; i++)
    {
        EFI_ERROR_SECTION_DESCRIPTOR secDesc;
        std::memcpy(&secDesc, &data[pos], sizeof(secDesc));
        pos += sizeof(secDesc);
        if (!decodeCperSection(data, eventDataSize, ampSpecHdr, &secDesc))
        {
            return false;
        }
    }

    return true;
}

void addCperSELLog(pldm_tid_t tid, uint16_t eventID, EFI_AMPERE_ERROR_DATA* p)
{
    std::vector<uint8_t> evtData;
    std::string message = "PLDM RAS SEL Event";
    uint8_t recordType = 0;
    uint8_t evtData1 = 0, evtData2 = 0, evtData3 = 0, evtData4 = 0,
            evtData5 = 0, evtData6 = 0;
    uint8_t socket = 0;

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

        bus.call(method);
    }
    catch (const std::exception& e)
    {
        lg2::error("call addCperSELLog error - {ERROR}", "ERROR", e);
    }
}

} // namespace oem_ampere
} // namespace pldm
