#include "cper.hpp"

#include "common/utils.hpp"

#include <string.h>

#include <phosphor-logging/lg2.hpp>

#include <cstring>
#include <iostream>
#include <map>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace oem
{

/*
 * Section type definitions, used in SectionType field in struct
 * cper_section_descriptor
 *
 * Processor Generic
 */
/* Processor Specific: ARM */
Guid CPER_SEC_PROC_ARM = {0xe19e3d16,
                          0xbc11,
                          0x11e4,
                          {0x9c, 0xaa, 0xc2, 0x05, 0x1d, 0x5d, 0x46, 0xb0}};

/* Platform Memory */
Guid CPER_SEC_PLATFORM_MEM = {0xa5bc1114,
                              0x6f64,
                              0x4ede,
                              {0xb8, 0x63, 0x3e, 0x83, 0xed, 0x7c, 0x83, 0xb1}};

/* PCIE */
Guid CPER_SEC_PCIE = {0xd995e954,
                      0xbbc1,
                      0x430f,
                      {0xad, 0x91, 0xb4, 0x4d, 0xcb, 0x3c, 0x6f, 0x35}};

/* Ampere Specific */
Guid CPER_AMPERE_SPECIFIC = {0x2826cc9f,
                             0x448c,
                             0x4c2b,
                             {0x86, 0xb6, 0xa9, 0x53, 0x94, 0xb7, 0xef, 0x33}};

std::map<uint16_t, size_t> armProcCtxMap = {
    {ARM_CONTEXT_TYPE_AARCH32_GPR, sizeof(ARM_V8_AARCH32_GPR)},
    {ARM_CONTEXT_TYPE_AARCH32_EL1, sizeof(ARM_AARCH32_EL1_CONTEXT_REGISTERS)},
    {ARM_CONTEXT_TYPE_AARCH32_EL2, sizeof(ARM_AARCH32_EL2_CONTEXT_REGISTERS)},
    {ARM_CONTEXT_TYPE_AARCH32_SECURE,
     sizeof(ARM_AARCH32_SECURE_CONTEXT_REGISTERS)},
    {ARM_CONTEXT_TYPE_AARCH64_GPR, sizeof(ARM_V8_AARCH64_GPR)},
    {ARM_CONTEXT_TYPE_AARCH64_EL1, sizeof(ARM_AARCH64_EL1_CONTEXT_REGISTERS)},
    {ARM_CONTEXT_TYPE_AARCH64_EL2, sizeof(ARM_AARCH64_EL2_CONTEXT_REGISTERS)},
    {ARM_CONTEXT_TYPE_AARCH64_EL3, sizeof(ARM_AARCH64_EL3_CONTEXT_REGISTERS)},
    {ARM_CONTEXT_TYPE_MISC, sizeof(ARM_MISC_CONTEXT_REGISTER)},
};

// Returns one if two EFI GUIDs are equal, zero otherwise.
static inline bool guidEqual(Guid* a, Guid* b)
{
    // Check top base 3 components.
    if (a->Data1 != b->Data1 || a->Data2 != b->Data2 || a->Data3 != b->Data3)
    {
        return false;
    }

    // Check Data4 array for equality.
    for (int i = 0; i < 8; i++)
    {
        if (a->Data4[i] != b->Data4[i])
            return false;
    }

    return true;
}

static void decodeSecAmpere(void* section, uint32_t len,
                            AmpereSpecData* ampSpecHdr, std::ofstream& out)
{
    std::memcpy(ampSpecHdr, section, sizeof(AmpereSpecData));
    out.write((char*)section, len);
}

static void decodeSecArm(void* section, AmpereSpecData* ampSpecHdr,
                         std::ofstream& out)
{
    int i, len;
    CPERSecProcArm* proc;
    CPERArmErrInfo* errInfo;
    CPERArmCtxInfo* ctxInfo;

    proc = (CPERSecProcArm*)section;
    out.write((char*)section, sizeof(CPERSecProcArm));
    errInfo = (CPERArmErrInfo*)(proc + 1);
    out.write((char*)errInfo, proc->ErrInfoNum * sizeof(CPERArmErrInfo));
    len = proc->SectionLength - (sizeof(CPERSecProcArm) +
                                 proc->ErrInfoNum * (sizeof(CPERArmErrInfo)));
    if (len < 0)
    {
        //        error("section length is too small,
        //        section_length={LENGTH}",
        //                   "LENGTH",  proc->SectionLength);
    }

    ctxInfo = (CPERArmCtxInfo*)(errInfo + proc->ErrInfoNum);
    for (i = 0; i < proc->ContextInfoNum; i++)
    {
        out.write((char*)ctxInfo, sizeof(CPERArmCtxInfo));
        out.write((char*)(ctxInfo + 1),
                  armProcCtxMap[ctxInfo->RegisterContextType]);
        int size = sizeof(CPERArmCtxInfo) + ctxInfo->RegisterArraySize;
        len -= size;
        ctxInfo = (CPERArmCtxInfo*)((long)ctxInfo + size);
    }

    if (len > 0)
    {
        /* Get Ampere Specific header data */
        std::memcpy(ampSpecHdr, (void*)ctxInfo, sizeof(AmpereSpecData));
        out.write((char*)ctxInfo, len);
    }
}

static void decodeSecPlatformMemory(void* section, AmpereSpecData* ampSpecHdr,
                                    std::ofstream& out)
{
    CPERSecMemErr* mem = (CPERSecMemErr*)section;
    out.write((char*)section, sizeof(CPERSecMemErr));
    if (mem->ErrorType == MEM_ERROR_TYPE_PARITY)
    {
        ampSpecHdr->typeId.member.ipType = ERROR_TYPE_ID_MCU;
        ampSpecHdr->subTypeId = SUBTYPE_ID_PARITY;
    }
}

static void decodeSecPcie(void* section, AmpereSpecData* ampSpecHdr,
                          std::ofstream& out)
{
    CPERSecPcieErr* pcieErr = (CPERSecPcieErr*)section;
    out.write((char*)section, sizeof(CPERSecPcieErr));
    if (pcieErr->ValidFields & CPER_PCIE_VALID_PORT_TYPE)
    {
        if (pcieErr->PortType == CPER_PCIE_PORT_TYPE_ROOT_PORT)
        {
            ampSpecHdr->subTypeId = ERROR_SUBTYPE_PCIE_AER_ROOT_PORT;
        }
        else
        {
            ampSpecHdr->subTypeId = ERROR_SUBTYPE_PCIE_AER_DEVICE;
        }
    }
}

static void decodeCperSection(
    const uint8_t* data, long basePos, AmpereSpecData* ampSpecHdr,
    CPERSectionDescriptor* secDesc, std::ofstream& out)
{
    long pos;

    // Read section as described by the section descriptor.
    pos = basePos + secDesc->SectionOffset;
    char* section = new char[secDesc->SectionLength];
    std::memcpy(section, &data[pos], secDesc->SectionLength);
    pos += secDesc->SectionLength;
    Guid* ptr = (Guid*)&secDesc->SectionType;
    if (guidEqual(ptr, &CPER_AMPERE_SPECIFIC))
    {
        info("RAS Section Type : Ampere Specific");
        decodeSecAmpere(section, secDesc->SectionLength, ampSpecHdr, out);
    }
    else if (guidEqual(ptr, &CPER_SEC_PROC_ARM))
    {
        info("RAS Section Type : ARM");
        decodeSecArm(section, ampSpecHdr, out);
    }
    else if (guidEqual(ptr, &CPER_SEC_PLATFORM_MEM))
    {
        info("RAS Section Type : Memory");
        decodeSecPlatformMemory(section, ampSpecHdr, out);
    }
    else if (guidEqual(ptr, &CPER_SEC_PCIE))
    {
        info("RAS Section Type : PCIE");
        decodeSecPcie(section, ampSpecHdr, out);
    }
    else
    {
        error("Section Type is not supported");
    }

    delete[] section;
}

void decodeCperRecord(const uint8_t* data, long pos, AmpereSpecData* ampSpecHdr,
                      std::ofstream& out)
{
    CPERRecodHeader cperHeader;
    int i;
    long basePos = pos;

    std::memcpy(&cperHeader, &data[pos], sizeof(CPERRecodHeader));
    pos += sizeof(CPERRecodHeader);

    // Revert 4 bytes of SignatureStart
    char* sigStr = (char*)&cperHeader.SignatureStart;
    char tmp;
    tmp = sigStr[0];
    sigStr[0] = sigStr[3];
    sigStr[3] = tmp;
    tmp = sigStr[1];
    sigStr[1] = sigStr[2];
    sigStr[2] = tmp;

    out.write((char*)&cperHeader, sizeof(CPERRecodHeader));

    CPERSectionDescriptor* secDesc =
        new CPERSectionDescriptor[cperHeader.SectionCount];
    for (i = 0; i < cperHeader.SectionCount; i++)
    {
        std::memcpy(&secDesc[i], &data[pos], sizeof(CPERSectionDescriptor));
        pos += sizeof(CPERSectionDescriptor);
        out.write((char*)&secDesc[i], sizeof(CPERSectionDescriptor));
    }

    for (i = 0; i < cperHeader.SectionCount; i++)
    {
        decodeCperSection(data, basePos, ampSpecHdr, &secDesc[i], out);
    }

    delete[] secDesc;
}

void addCperSELLog(uint8_t TID, uint16_t eventID, AmpereSpecData* p)
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
    socket = (TID == 1) ? 0 : 1;
    recordType = 0xD0;
    evtData1 = SENSOR_TYPE_OEM | socket;
    evtData2 = eventID;
    evtData3 = p->typeId.type >> 8;
    evtData4 = p->typeId.type;
    evtData5 = p->subTypeId >> 8;
    evtData6 = p->subTypeId;
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
        auto method = bus.new_method_call(
            "xyz.openbmc_project.Logging.IPMI",
            "/xyz/openbmc_project/Logging/IPMI",
            "xyz.openbmc_project.Logging.IPMI", "IpmiSelAddOem");
        method.append(message, evtData, recordType);

        auto selReply = bus.call(method);
        if (selReply.is_method_error())
        {
            error("addCperSELLog: add SEL log error");
        }
    }
    catch (const std::exception& e)
    {
        error("call addCperSELLog error - {ERROR}", "ERROR", e);
    }
}

} // namespace oem
} // namespace pldm
