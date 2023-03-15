#include "cper.hpp"

#include <string.h>

#include <cstring>
#include <iostream>
#include <map>
#include <vector>
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
        std::cerr << "section length is too small : " << proc->SectionLength
                  << "\n";
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

static void decodeSecPcie(void* section, AmpereSpecData* /*ampSpecHdr*/,
                          std::ofstream& out)
{
    out.write((char*)section, sizeof(CPERSecPcieErr));
}

static void decodeCperSection(std::vector<uint8_t>& data, long basePos,
                              AmpereSpecData* ampSpecHdr,
                              CPERSectionDescriptor* secDesc,
                              std::ofstream& out)
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
        std::cout << "RAS Section Type : Ampere Specific\n";
        decodeSecAmpere(section, secDesc->SectionLength, ampSpecHdr, out);
    }
    else if (guidEqual(ptr, &CPER_SEC_PROC_ARM))
    {
        std::cout << "RAS Section Type : ARM\n";
        decodeSecArm(section, ampSpecHdr, out);
    }
    else if (guidEqual(ptr, &CPER_SEC_PLATFORM_MEM))
    {
        std::cout << "RAS Section Type : Memory\n";
        decodeSecPlatformMemory(section, ampSpecHdr, out);
    }
    else if (guidEqual(ptr, &CPER_SEC_PCIE))
    {
        std::cout << "RAS Section Type : PCIE\n";
        decodeSecPcie(section, ampSpecHdr, out);
    }
    else
    {
        std::cerr << "Section Type: " << std::hex << ptr->Data1 << "-"
                  << ptr->Data2 << "-" << ptr->Data3 << "-"
                  << *(uint64_t*)ptr->Data4 << "not support\n";
    }

    delete[] section;
}

void decodeCperRecord(std::vector<uint8_t>& data, long pos,
                      AmpereSpecData* ampSpecHdr, std::ofstream& out)
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
