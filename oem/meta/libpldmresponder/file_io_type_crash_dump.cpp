#include "file_io_type_crash_dump.hpp"

#include "common/utils.hpp"

#include <fcntl.h>
#include <sys/file.h>

#include <phosphor-logging/lg2.hpp>

#include <fstream>
#include <regex>

constexpr uint8_t ccmNum = 8;
constexpr uint8_t tcdxNum = 12;
constexpr uint8_t cakeNum = 6;
constexpr uint8_t pie0Num = 1;
constexpr uint8_t iomNum = 4;
constexpr uint8_t ccixNum = 4;
constexpr uint8_t csNum = 8;

enum class BankType : uint8_t
{
    mca = 0x01,
    virt = 0x02,
    cpuWdt = 0x03,
    tcdx = 0x06,
    cake = 0x07,
    pie0 = 0x08,
    iom = 0x09,
    ccix = 0x0a,
    cs = 0x0b,
    pcieAer = 0x0c,
    wdtReg = 0x0d,
    ctrl = 0x80,
    crdHdr = 0x81
};

enum class CrdState
{
    free = 0x01,
    waitData = 0x02,
    packing = 0x03
};

enum class CrdCtrl
{
    getState = 0x01,
    finish = 0x02
};

#pragma pack(push, 1)
struct BankCorePair
{
    uint8_t bankId;
    uint8_t coreId;
};

struct CrdCmdHdr
{
    uint8_t version;
    uint8_t reserved[3];
};

struct CrdBankHdr
{
    BankType bankType;
    uint8_t version;
    union
    {
        BankCorePair mcaList;
        uint8_t reserved[2];
    };
};

struct CrashDumpHdr
{
    CrdCmdHdr cmdHdr;
    CrdBankHdr bankHdr;
};

// Type 0x01: MCA Bank
struct CrdMcaBank
{
    uint64_t mcaCtrl;
    uint64_t mcaSts;
    uint64_t mcaAddr;
    uint64_t mcaMisc0;
    uint64_t mcaCtrlMask;
    uint64_t mcaConfig;
    uint64_t mcaIpid;
    uint64_t mcaSynd;
    uint64_t mcaDestat;
    uint64_t mcaDeaddr;
    uint64_t mcaMisc1;
};

// Type 0x02: Virtual/Global Bank
struct CrdVirtualBankV2
{
    uint32_t s5ResetSts;
    uint32_t breakevent;
    uint16_t mcaCount;
    uint16_t procNum;
    uint32_t apicId;
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    // struct BankCorePair mcaList[];
};

struct CrdVirtualBankV3
{
    uint32_t s5ResetSts;
    uint32_t breakevent;
    uint32_t rstSts;
    uint16_t mcaCount;
    uint16_t procNum;
    uint32_t apicId;
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    // struct BankCorePair mcaList[];
};

// Type 0x03: CPU/Data Fabric Watchdog Timer Bank
struct CrdCpuWdtBank
{
    uint32_t hwAssertStsHi[ccmNum];
    uint32_t hwAssertStsLo[ccmNum];
    uint32_t origWdtAddrLogHi[ccmNum];
    uint32_t origWdtAddrLogLo[ccmNum];
    uint32_t hwAssertMskHi[ccmNum];
    uint32_t hwAssertMskLo[ccmNum];
    uint32_t origWdtAddrLogStat[ccmNum];
};

template <size_t N>
struct CrdHwAssertBank
{
    uint32_t hwAssertStsHi[N];
    uint32_t hwAssertStsLo[N];
    uint32_t hwAssertMskHi[N];
    uint32_t hwAssertMskLo[N];
};

// Type 0x0C: PCIe AER Bank
struct CrdPcieAerBank
{
    uint8_t bus;
    uint8_t dev;
    uint8_t fun;
    uint16_t cmd;
    uint16_t sts;
    uint16_t slot;
    uint8_t secondBus;
    uint16_t vendorId;
    uint16_t devId;
    uint16_t classCodeLo; // Class Code 3 byte
    uint8_t classCodeHi;
    uint16_t secondSts;
    uint16_t ctrl;
    uint32_t uncorrErrSts;
    uint32_t uncorrErrMsk;
    uint32_t uncorrErrSeverity;
    uint32_t corrErrSts;
    uint32_t corrErrMsk;
    uint32_t hdrLogDw0;
    uint32_t hdrLogDw1;
    uint32_t hdrLogDw2;
    uint32_t hdrLogDw3;
    uint32_t rootErrSts;
    uint16_t corrErrSrcId;
    uint16_t errSrcId;
    uint32_t laneErrSts;
};

// Type 0x0D: SMU/PSP/PTDMA Watchdog Timers Register Bank
struct CrdWdtRegBank
{
    uint8_t nbio;
    char name[32];
    uint32_t addr;
    uint8_t count;
    // uint32_t data[];
};

// Type 0x81: Crashdump Header
struct CrdHdrBank
{
    uint64_t ppin;
    uint32_t ucodeVer;
    uint32_t pmio;
};
#pragma pack(pop)

PHOSPHOR_LOG2_USING;
namespace pldm::responder::oem_meta
{

static int handleMcaBank(const CrashDumpHdr& hdr, std::span<const uint8_t> data,
                         std::stringstream& ss)
{
    if (data.size() < sizeof(CrdMcaBank))
    {
        error("Invalid length for MCA Bank");
        return PLDM_ERROR;
    }

    const auto* pBank = reinterpret_cast<const CrdMcaBank*>(data.data());
    ss << std::format(" Bank ID : 0x{:02X}, Core ID : 0x{:02X}\n",
                      hdr.bankHdr.mcaList.bankId, hdr.bankHdr.mcaList.coreId);
    ss << std::format(" MCA_CTRL      : 0x{:016X}\n", pBank->mcaCtrl);
    ss << std::format(" MCA_STATUS    : 0x{:016X}\n", pBank->mcaSts);
    ss << std::format(" MCA_ADDR      : 0x{:016X}\n", pBank->mcaAddr);
    ss << std::format(" MCA_MISC0     : 0x{:016X}\n", pBank->mcaMisc0);
    ss << std::format(" MCA_CTRL_MASK : 0x{:016X}\n", pBank->mcaCtrlMask);
    ss << std::format(" MCA_CONFIG    : 0x{:016X}\n", pBank->mcaConfig);
    ss << std::format(" MCA_IPID      : 0x{:016X}\n", pBank->mcaIpid);
    ss << std::format(" MCA_SYND      : 0x{:016X}\n", pBank->mcaSynd);
    ss << std::format(" MCA_DESTAT    : 0x{:016X}\n", pBank->mcaDestat);
    ss << std::format(" MCA_DEADDR    : 0x{:016X}\n", pBank->mcaDeaddr);
    ss << std::format(" MCA_MISC1     : 0x{:016X}\n", pBank->mcaMisc1);
    ss << "\n";

    return PLDM_SUCCESS;
}

template <typename T>
static int handleVirtualBank(std::span<const uint8_t> data,
                             std::stringstream& ss)
{
    if (data.size() < sizeof(T))
    {
        error("Invalid length1 for Virtual Bank");
        return PLDM_ERROR;
    }

    const auto* pBank = reinterpret_cast<const T*>(data.data());
    const uint8_t* mcaListStart = data.data() + sizeof(T);

    // Verify total data size matches the expected size
    size_t requiredSize = sizeof(T) + sizeof(BankCorePair) * pBank->mcaCount;
    if (data.size() < requiredSize)
    {
        error("Invalid length for Virtual Bank");
        return PLDM_ERROR;
    }

    // Populate a vector from the raw data
    std::vector<BankCorePair> mcaList(
        reinterpret_cast<const BankCorePair*>(mcaListStart),
        reinterpret_cast<const BankCorePair*>(
            mcaListStart + sizeof(BankCorePair) * pBank->mcaCount));

    ss << " Virtual Bank\n";
    ss << std::format(" S5_RESET_STATUS   : 0x{:08X}\n", pBank->s5ResetSts);
    ss << std::format(" PM_BREAKEVENT     : 0x{:08X}\n", pBank->breakevent);
    if constexpr (std::is_same_v<T, CrdVirtualBankV3>)
    {
        ss << std::format(" WARMCOLDRSTSTATUS : 0x{:08X}\n", pBank->rstSts);
    }
    ss << std::format(" PROCESSOR NUMBER  : 0x{:04X}\n", pBank->procNum);
    ss << std::format(" APIC ID           : 0x{:08X}\n", pBank->apicId);
    ss << std::format(" EAX               : 0x{:08X}\n", pBank->eax);
    ss << std::format(" EBX               : 0x{:08X}\n", pBank->ebx);
    ss << std::format(" ECX               : 0x{:08X}\n", pBank->ecx);
    ss << std::format(" EDX               : 0x{:08X}\n", pBank->edx);
    ss << " VALID LIST        : ";
    for (const auto& pair : mcaList)
    {
        ss << std::format("(0x{:02X},0x{:02X}) ", pair.bankId, pair.coreId);
    }
    ss << "\n\n";

    return PLDM_SUCCESS;
}

static int handleCpuWdtBank(std::span<const uint8_t> data,
                            std::stringstream& ss)
{
    if (data.size() < sizeof(CrdCpuWdtBank))
    {
        error("Invalid length for CPU WDT Bank");
        return PLDM_ERROR;
    }

    const auto* pBank = reinterpret_cast<const CrdCpuWdtBank*>(data.data());
    for (size_t i = 0; i < ccmNum; i++)
    {
        ss << std::format("  [CCM{}]\n", i);
        ss << std::format("    HwAssertStsHi      : 0x{:08X}\n",
                          pBank->hwAssertStsHi[i]);
        ss << std::format("    HwAssertStsLo      : 0x{:08X}\n",
                          pBank->hwAssertStsLo[i]);
        ss << std::format("    OrigWdtAddrLogHi   : 0x{:08X}\n",
                          pBank->origWdtAddrLogHi[i]);
        ss << std::format("    OrigWdtAddrLogLo   : 0x{:08X}\n",
                          pBank->origWdtAddrLogLo[i]);
        ss << std::format("    HwAssertMskHi      : 0x{:08X}\n",
                          pBank->hwAssertMskHi[i]);
        ss << std::format("    HwAssertMskLo      : 0x{:08X}\n",
                          pBank->hwAssertMskLo[i]);
        ss << std::format("    OrigWdtAddrLogStat : 0x{:08X}\n",
                          pBank->origWdtAddrLogStat[i]);
    }
    ss << "\n";

    return PLDM_SUCCESS;
}

template <size_t N>
static int handleHwAssertBank(const char* name, std::span<const uint8_t> data,
                              std::stringstream& ss)
{
    if (data.size() < sizeof(CrdHwAssertBank<N>))
    {
        error("Invalid length for HwAssert Bank");
        return PLDM_ERROR;
    }

    const CrdHwAssertBank<N>* pBank =
        reinterpret_cast<const CrdHwAssertBank<N>*>(data.data());

    for (size_t i = 0; i < N; i++)
    {
        ss << std::format("  [{}{}]\n", name, i);
        ss << std::format("    HwAssertStsHi : 0x{:08X}\n",
                          pBank->hwAssertStsHi[i]);
        ss << std::format("    HwAssertStsLo : 0x{:08X}\n",
                          pBank->hwAssertStsLo[i]);
        ss << std::format("    HwAssertMskHi : 0x{:08X}\n",
                          pBank->hwAssertMskHi[i]);
        ss << std::format("    HwAssertMskLo : 0x{:08X}\n",
                          pBank->hwAssertMskLo[i]);
    }
    ss << "\n";

    return PLDM_SUCCESS;
}

static int handlePcieAerBank(std::span<const uint8_t> data,
                             std::stringstream& ss)
{
    if (data.size() < sizeof(CrdPcieAerBank))
    {
        error("Invalid length for PCIe AER Bank");
        return PLDM_ERROR;
    }

    const auto* pBank = reinterpret_cast<const CrdPcieAerBank*>(data.data());
    ss << std::format("  [Bus{} Dev{} Fun{}]\n", pBank->bus, pBank->dev,
                      pBank->fun);
    ss << std::format("    Command                      : 0x{:04X}\n",
                      pBank->cmd);
    ss << std::format("    Status                       : 0x{:04X}\n",
                      pBank->sts);
    ss << std::format("    Slot                         : 0x{:04X}\n",
                      pBank->slot);
    ss << std::format("    Secondary Bus                : 0x{:02X}\n",
                      pBank->secondBus);
    ss << std::format("    Vendor ID                    : 0x{:04X}\n",
                      pBank->vendorId);
    ss << std::format("    Device ID                    : 0x{:04X}\n",
                      pBank->devId);
    ss << std::format("    Class Code                   : 0x{:02X}{:04X}\n",
                      pBank->classCodeHi, pBank->classCodeLo);
    ss << std::format("    Bridge: Secondary Status     : 0x{:04X}\n",
                      pBank->secondSts);
    ss << std::format("    Bridge: Control              : 0x{:04X}\n",
                      pBank->ctrl);
    ss << std::format("    Uncorrectable Error Status   : 0x{:08X}\n",
                      pBank->uncorrErrSts);
    ss << std::format("    Uncorrectable Error Mask     : 0x{:08X}\n",
                      pBank->uncorrErrMsk);
    ss << std::format("    Uncorrectable Error Severity : 0x{:08X}\n",
                      pBank->uncorrErrSeverity);
    ss << std::format("    Correctable Error Status     : 0x{:08X}\n",
                      pBank->corrErrSts);
    ss << std::format("    Correctable Error Mask       : 0x{:08X}\n",
                      pBank->corrErrMsk);
    ss << std::format("    Header Log DW0               : 0x{:08X}\n",
                      pBank->hdrLogDw0);
    ss << std::format("    Header Log DW1               : 0x{:08X}\n",
                      pBank->hdrLogDw1);
    ss << std::format("    Header Log DW2               : 0x{:08X}\n",
                      pBank->hdrLogDw2);
    ss << std::format("    Header Log DW3               : 0x{:08X}\n",
                      pBank->hdrLogDw3);
    ss << std::format("    Root Error Status            : 0x{:08X}\n",
                      pBank->rootErrSts);
    ss << std::format("    Correctable Error Source ID  : 0x{:04X}\n",
                      pBank->corrErrSrcId);
    ss << std::format("    Error Source ID              : 0x{:04X}\n",
                      pBank->errSrcId);
    ss << std::format("    Lane Error Status            : 0x{:08X}\n",
                      pBank->laneErrSts);
    ss << "\n";

    return PLDM_SUCCESS;
}

static int handleWdtRegBank(std::span<const uint8_t> data,
                            std::stringstream& ss)
{
    if (data.size() < sizeof(CrdWdtRegBank))
    {
        error("Invalid length for WDT Register Bank");
        return PLDM_ERROR;
    }

    const auto* pBank = reinterpret_cast<const CrdWdtRegBank*>(data.data());
    if (data.size() < sizeof(CrdWdtRegBank) + sizeof(uint32_t) * pBank->count)
    {
        error("Invalid length for WDT Register Bank");
        return PLDM_ERROR;
    }

    const uint32_t* dataStart =
        reinterpret_cast<const uint32_t*>(data.data() + sizeof(CrdWdtRegBank));
    std::span<const uint32_t> dataSpan(dataStart, pBank->count);

    ss << std::format("  [NBIO{}] {}\n", pBank->nbio, pBank->name);
    ss << std::format("    Address: 0x{:08X}\n", pBank->addr);
    ss << std::format("    Data Count: {}\n", pBank->count);
    ss << "    Data:\n";
    for (size_t i = 0; i < dataSpan.size(); i++)
    {
        ss << std::format("      {}: 0x{:08X}\n", i, dataSpan[i]);
    }
    ss << "\n";

    return PLDM_SUCCESS;
}

static int handleCrdHdrBank(std::span<const uint8_t> data,
                            std::stringstream& ss)
{
    if (data.size() < sizeof(CrdHdrBank))
    {
        error("Invalid length for Crashdump Header");
        return PLDM_ERROR;
    }

    const auto* pBank = reinterpret_cast<const CrdHdrBank*>(data.data());
    ss << " Crashdump Header\n";
    ss << std::format(" CPU PPIN      : 0x{:016X}\n", pBank->ppin);
    ss << std::format(" UCODE VERSION : 0x{:08X}\n", pBank->ucodeVer);
    ss << std::format(" PMIO 80h      : 0x{:08X}\n", pBank->pmio);
    ss << std::format(
        "    BIT0 - SMN Parity/SMN Timeouts PSP/SMU Parity and ECC/SMN On-Package Link Error : {}\n",
        pBank->pmio & 0x1);
    ss << std::format("    BIT2 - PSP Parity and ECC : {}\n",
                      (pBank->pmio & 0x4) >> 2);
    ss << std::format("    BIT3 - SMN Timeouts SMU : {}\n",
                      (pBank->pmio & 0x8) >> 3);
    ss << std::format("    BIT4 - SMN Off-Package Link Packet Error : {}\n",
                      (pBank->pmio & 0x10) >> 4);
    ss << "\n";

    return PLDM_SUCCESS;
}

static std::string getFilename(const std::filesystem::path& dir,
                               const std::string& prefix)
{
    std::vector<int> indices;
    std::regex pattern(prefix + "(\\d+)\\.txt");

    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        std::string filename = entry.path().filename().string();
        std::smatch match;
        if (std::regex_match(filename, match, pattern))
            indices.push_back(std::stoi(match[1]));
    }

    std::sort(indices.rbegin(), indices.rend());
    while (indices.size() > 2) // keep 3 files, so remove if more than 2
    {
        std::filesystem::remove(
            dir / (prefix + std::to_string(indices.back()) + ".txt"));
        indices.pop_back();
    }

    int nextIndex = indices.empty() ? 1 : indices.front() + 1;
    return prefix + std::to_string(nextIndex) + ".txt";
}

int CrashDumpHandler::handleCtrlBank(std::span<const uint8_t> data,
                                     std::stringstream& ss)
{
    if (data.empty())
    {
        error("Invalid length for Control Bank");
        return PLDM_ERROR;
    }

    switch (static_cast<CrdCtrl>(data[0]))
    {
        case CrdCtrl::getState:
            break;
        case CrdCtrl::finish:
        {
            const std::filesystem::path dumpDir = "/var/lib/fb-ipmi-oem";
            auto prefix = "host" + getSlotNumberString() + "_crashdump_";
            std::string filename = getFilename(dumpDir, prefix);
            std::ofstream outFile(dumpDir / filename);
            if (!outFile.is_open())
                return PLDM_ERROR;

            auto now = std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now());
            outFile << "Crash Dump generated at: "
                    << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S")
                    << "\n\n";
            outFile << ss.str();
            outFile.close();
            ss.str("");
            ss.clear();
            auto filePath = dumpDir / filename;
            auto error = "Crashdump for Host: " + getSlotNumberString() +
                         " is generated at " + filePath.string();
            pldm::utils::reportError(error.c_str());
            break;
        }
        default:
            error("Invalid Control Bank command");
            return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

std::string CrashDumpHandler::getSlotNumberString()
{
    static constexpr auto slotInterface =
        "xyz.openbmc_project.Inventory.Decorator.Slot";
    static constexpr auto slotNumberProperty = "SlotNumber";

    std::vector<std::string> slotInterfaceList = {slotInterface};
    pldm::utils::GetAncestorsResponse response;

    for (const auto& [configDbusPath, mctpEndpoint] : configurations)
    {
        if (mctpEndpoint.EndpointId == tid)
        {
            try
            {
                response = pldm::utils::DBusHandler().getAncestors(
                    configDbusPath.c_str(), slotInterfaceList);
                if (response.size() != 1)
                {
                    error(
                        "{FUNC}: Only Board layer should have Decorator.Slot interface, got {SIZE} Dbus Object(s) have interface Decorator.Slot}",
                        "FUNC", std::string(__func__), "SIZE", response.size());
                    throw std::runtime_error(
                        "Invalid Response of GetAncestors");
                }
            }
            catch (const sdbusplus::exception_t& e)
            {
                error(
                    "GetAncestors call failed with, ERROR={ERROR} PATH={PATH} INTERFACE={INTERFACE}",
                    "ERROR", e, "PATH", configDbusPath.c_str(), "INTERFACE",
                    slotInterface);
            }

            std::string slotNum{};
            try
            {
                auto result =
                    pldm::utils::DBusHandler().getDbusProperty<uint64_t>(
                        std::get<0>(response.front()).c_str(),
                        slotNumberProperty, slotInterface);
                slotNum = std::to_string(result);
            }
            catch (const sdbusplus::exception_t& e)
            {
                error(
                    "Error getting Names property, ERROR={ERROR} PATH={PATH} INTERFACE={INTERFACE}",
                    "ERROR", e, "PATH", std::get<0>(response.front()).c_str(),
                    "INTERFACE", slotInterface);
            }
            catch (const std::runtime_error& e)
            {
                error("{FUNC}: Runtime error occurred, ERROR={ERROR}", "FUNC",
                      std::string(__func__), "ERROR", e.what());
            }
            catch (const std::exception& e)
            {
                error("{FUNC}: Unexpected exception occurred, ERROR={ERROR}",
                      "FUNC", std::string(__func__), "ERROR", e.what());
            }
            return slotNum;
        }
    }

    throw std::runtime_error(
        std::string(__func__) +
        ": No matching EndpointId found for tid=" + std::to_string(tid));
}

int CrashDumpHandler::write(const message& data)
{
    static std::stringstream ss;

    if (data.size() < sizeof(CrashDumpHdr))
    {
        error("Invalid length for crash dump header");
        return PLDM_ERROR;
    }

    const auto* pHdr = reinterpret_cast<const CrashDumpHdr*>(data.data());
    std::span<const uint8_t> bData{data.data() + sizeof(CrashDumpHdr),
                                   data.size() - sizeof(CrashDumpHdr)};
    int res;

    switch (pHdr->bankHdr.bankType)
    {
        case BankType::mca:
            res = handleMcaBank(*pHdr, bData, ss);
            break;
        case BankType::virt:
            if (pHdr->bankHdr.version >= 3)
            {
                res = handleVirtualBank<CrdVirtualBankV3>(bData, ss);
                break;
            }
            res = handleVirtualBank<CrdVirtualBankV2>(bData, ss);
            break;
        case BankType::cpuWdt:
            res = handleCpuWdtBank(bData, ss);
            break;
        case BankType::tcdx:
            res = handleHwAssertBank<tcdxNum>("TCDX", bData, ss);
            break;
        case BankType::cake:
            res = handleHwAssertBank<cakeNum>("CAKE", bData, ss);
            break;
        case BankType::pie0:
            res = handleHwAssertBank<pie0Num>("PIE", bData, ss);
            break;
        case BankType::iom:
            res = handleHwAssertBank<iomNum>("IOM", bData, ss);
            break;
        case BankType::ccix:
            res = handleHwAssertBank<ccixNum>("CCIX", bData, ss);
            break;
        case BankType::cs:
            res = handleHwAssertBank<csNum>("CS", bData, ss);
            break;
        case BankType::pcieAer:
            res = handlePcieAerBank(bData, ss);
            break;
        case BankType::wdtReg:
            res = handleWdtRegBank(bData, ss);
            break;
        case BankType::ctrl:
            res = handleCtrlBank(bData, ss);
            break;
        case BankType::crdHdr:
            res = handleCrdHdrBank(bData, ss);
            if (res == PLDM_ERROR)
            {
                error("Fail to decode crash dump data");
            }
            break;
        default:
            error("Unsupported bank type");
            return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

} // namespace pldm::responder::oem_meta
