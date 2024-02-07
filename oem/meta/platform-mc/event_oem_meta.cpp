#include "event_oem_meta.hpp"

#include "common/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cstdint>
#include <iostream>
#include <sstream>

namespace pldm
{
namespace platform_mc
{
namespace oem_meta
{

constexpr auto slotInterface = "xyz.openbmc_project.Inventory.Decorator.Slot";
constexpr auto slotNumberProperty = "SlotNumber";

void covertToDimmString(uint8_t cpu, uint8_t channel, uint8_t slot,
                        std::string& str)
{
    constexpr char label[] = {'A', 'C', 'B', 'D'};
    constexpr size_t labelSize = sizeof(label);

    size_t idx = cpu * 2 + slot;
    if (idx < labelSize)
    {
        str = label[idx] + std::to_string(channel);
    }
    else
    {
        str = "NA";
    }
}

void getCommonDimmLocation(const _dimm_info& dimmInfo,
                           std::string& dimmLocation, std::string& dimm)
{
    std::string sled_str = std::to_string(dimmInfo.sled);
    std::string socket_str = std::to_string(dimmInfo.socket);

    // Check Channel and Slot
    if (dimmInfo.channel == 0xFF && dimmInfo.slot == 0xFF)
    {
        dimm = "unknown";
        dimmLocation = "DIMM Slot Location: Sled " + sled_str + "/Socket " +
                       socket_str +
                       ", Channel unknown, Slot unknown, DIMM unknown";
    }
    else
    {
        uint8_t channel = dimmInfo.channel & 0x0F;
        uint8_t slot = dimmInfo.slot & 0x07;
        covertToDimmString(dimmInfo.socket, channel, slot, dimm);

        std::string channel_str = std::to_string(channel);
        std::string slot_str = std::to_string(slot);

        dimmLocation = "DIMM Slot Location: Sled " + sled_str + "/Socket " +
                       socket_str + ", Channel " + channel_str + ", Slot " +
                       slot_str + ", DIMM " + dimm;
    }
}

std::string to_hex_string(uint8_t value)
{
    std::stringstream ss;
    ss << std::hex << static_cast<unsigned int>(value);
    return ss.str();
}

std::string
    getSlotNumber(tid_t tid,
                  const std::map<std::string, MctpEndpoint>& configurations)
{
    std::string slotNumber = "Unknown";
    for (const auto& [configDbusPath, mctpEndpoint] : configurations)
    {
        if (mctpEndpoint.EndpointId == tid)
        {
            std::vector<std::string> slotInterfaceList = {slotInterface};
            pldm::utils::GetAncestorsResponse response;
            std::string endpointDbusPath;
            try
            {
                response = pldm::utils::DBusHandler().getAncestors(
                    configDbusPath.c_str(), slotInterfaceList);
                if (response.size() != 1)
                {
                    lg2::error(
                        "{FUNC}: Only Board layer should have Decorator.Slot interface, got {SIZE} Dbus Object(s) have interface Decorator.Slot}",
                        "FUNC", std::string(__func__), "SIZE", response.size());
                    return slotNumber; // return "Unknown"
                }
                endpointDbusPath = std::get<0>(response.front());
            }
            catch (const sdbusplus::exception_t& e)
            {
                lg2::error("{FUNC}: Failed to call GetAncestors, ERROR={ERROR}",
                           "FUNC", std::string(__func__), "ERROR", e.what());
                return slotNumber; // return "Unknown";
            }

            try
            {
                auto number =
                    pldm::utils::DBusHandler().getDbusProperty<uint64_t>(
                        endpointDbusPath.c_str(), slotNumberProperty,
                        slotInterface);
                slotNumber = std::to_string(number);
            }
            catch (const sdbusplus::exception_t& e)
            {
                lg2::error("{FUNC}: Failed to execute Dbus call, ERROR={ERROR}",
                           "FUNC", std::string(__func__), "ERROR", e.what());
                return slotNumber; // return "Unknown"
            }
        }
    }
    return slotNumber;
}

int processOemMetaEvent(
    tid_t tid, const uint8_t* eventData, [[maybe_unused]] size_t eventDataSize,
    const std::map<std::string, MctpEndpoint>& configurations)
{
    enum : uint8_t
    {
        UNIFIED_PCIE_ERR = 0x0,
        UNIFIED_MEM_ERR = 0x1,
        UNIFIED_UPI_ERR = 0x2,
        UNIFIED_IIO_ERR = 0x3,
        UNIFIED_RP_PIO_1st = 0x6,
        UNIFIED_RP_PIO_2nd = 0x7,
        UNIFIED_POST_ERR = 0x8,
        UNIFIED_PCIE_EVENT = 0x9,
        UNIFIED_MEM_EVENT = 0xA,
        UNIFIED_UPI_EVENT = 0xB,
        UNIFIED_BOOT_GUARD = 0xC,
        UNIFIED_PPR_EVENT = 0xD,
        UNIFIED_CXL_MEM_ERR = 0xE,
    };

    enum : uint8_t
    {
        MEMORY_TRAINING_ERR = 0x0,
        MEMORY_CORRECTABLE_ERR = 0x1,
        MEMORY_UNCORRECTABLE_ERR = 0x2,
        MEMORY_CORR_ERR_PTRL_SCR = 0x3,
        MEMORY_UNCORR_ERR_PTRL_SCR = 0x4,
        MEMORY_PARITY_ERR_PCC0 = 0x5,
        MEMORY_PARITY_ERR_PCC1 = 0x6,
        MEMORY_PMIC_ERR = 0x7,
    };

    enum : uint8_t
    {
        POST_PXE_BOOT_FAIL = 0x0,
        POST_CMOS_CLEARED = 0x1,
        POST_TPM_SELF_TEST_FAIL = 0x2,
        POST_BOOT_DRIVE_FAIL = 0x3,
        POST_DATA_DRIVE_FAIL = 0x4,
        POST_INVALID_BOOT_ORDER = 0x5,
        POST_HTTP_BOOT_FAIL = 0x6,
        POST_GET_CERT_FAIL = 0x7,
        POST_AMD_ABL_FAIL = 0xA,
    };

    enum : uint8_t
    {
        PCIE_DPC = 0x0,
    };

    enum : uint8_t
    {
        MEM_PPR = 0x0,
        MEM_ADDDC = 0x5,
        MEM_NO_DIMM = 0x7,
    };

    enum : uint8_t
    {
        UPI_INIT_ERR = 0x0,
    };

    std::vector<std::string> memoryError = {
        "Memory training failure",
        "Memory correctable error",
        "Memory uncorrectable error",
        "Memory correctable error (Patrol scrub)",
        "Memory uncorrectable error (Patrol scrub)",
        "Memory Parity Error (PCC=0)",
        "Memory Parity Error (PCC=1)",
        "Memory PMIC Error",
        "CXL Memory training error",
        "Reserved"};

    std::vector<std::string> certEvent = {
        "No certificate at BMC", "IPMI transaction fail",
        "Certificate data corrupted", "Reserved"};

    std::vector<std::string> postError = {
        "System PXE boot fail",
        "CMOS/NVRAM configuration cleared",
        "TPM Self-Test Fail",
        "Boot Drive failure",
        "Data Drive failure",
        "Received invalid boot order request from BMC",
        "System HTTP boot fail",
        "BIOS fails to get the certificate from BMC",
        "Password cleared by jumper",
        "DXE FV check failure",
        "AMD ABL failure",
        "Reserved"};

    std::vector<std::string> pcieEvent = {"PCIe DPC Event",
                                          "PCIe LER Event",
                                          "PCIe Link Retraining and Recovery",
                                          "PCIe Link CRC Error Check and Retry",
                                          "PCIe Corrupt Data Containment",
                                          "PCIe Express ECRC",
                                          "Reserved"};

    std::vector<std::string> memoryEvent = {
        "Memory PPR event",
        "Memory Correctable Error logging limit reached",
        "Memory disable/map-out for FRB",
        "Memory SDDC",
        "Memory Address range/Partial mirroring",
        "Memory ADDDC",
        "Memory SMBus hang recovery",
        "No DIMM in System",
        "Reserved"};

    std::vector<std::string> memoryPprRepairTime = {"Boot time", "Autonomous",
                                                    "Run time", "Reserved"};

    std::vector<std::string> memoryPprEvent = {"PPR success", "PPR fail",
                                               "PPR request", "Reserved"};

    std::vector<std::string> memoryAdddcEvent = {
        "Bank VLS", "r-Bank VLS + re-buddy", "r-Bank VLS + Rank VLS",
        "r-Rank VLS + re-buddy"};

    std::vector<std::string> upiEvent = {
        "Successful LLR without Phy Reinit", "Successful LLR with Phy Reinit",
        "COR Phy Lane failure, recovery in x8 width", "Reserved"};

    std::vector<std::string> pprEvent = {"PPR disable", "Soft PPR", "Hard PPR"};

    std::vector<std::string> upiError = {"UPI Init error", "Reserved"};

    uint8_t generalInfo = eventData[0];
    uint8_t errorType = generalInfo & 0xF;
    _dimm_info dimmInfo = {
        static_cast<uint8_t>((eventData[5] >> 4) & 0x03), // Sled
        static_cast<uint8_t>(eventData[5] & 0x0F),        // Socket
        static_cast<uint8_t>(eventData[6]),               // Channel
        static_cast<uint8_t>(eventData[7])                // Slot
    };
    std::string errorLog;
    switch (errorType)
    {
        case UNIFIED_PCIE_ERR:
        {
            uint8_t plat = (generalInfo & 0x10) >> 4;
            if (plat == 0)
            { // x86
                errorLog =
                    "GeneralInfo: x86/PCIeErr(0x" + to_hex_string(generalInfo) +
                    "), Bus " + to_hex_string(eventData[8]) + "/Dev " +
                    to_hex_string(eventData[7] >> 3) + "/Fun " +
                    to_hex_string(eventData[7] & 0x7) + ", TotalErrID1Cnt: 0x" +
                    to_hex_string((eventData[10] << 8) | eventData[9]) +
                    ", ErrID2: 0x" + to_hex_string(eventData[11]) +
                    ", ErrID1: 0x" + to_hex_string(eventData[12]);
            }
            else
            {
                errorLog = "GeneralInfo: ARM/PCIeErr(0x" +
                           to_hex_string(generalInfo) + "), Aux. Info: 0x" +
                           to_hex_string((eventData[6] << 8) | eventData[5]) +
                           ", Bus " + to_hex_string(eventData[8]) + "/Dev " +
                           to_hex_string(eventData[7] >> 3) + "/Fun " +
                           to_hex_string(eventData[7] & 0x7) +
                           ", TotalErrID1Cnt: 0x" +
                           to_hex_string((eventData[10] << 8) | eventData[9]) +
                           ", ErrID2: 0x" + to_hex_string(eventData[11]) +
                           ", ErrID1: 0x" + to_hex_string(eventData[12]);
            }
            break;
        }
        case UNIFIED_MEM_ERR:
        {
            std::string dimmLocation, dimm;
            getCommonDimmLocation(dimmInfo, dimmLocation, dimm);
            uint8_t plat = (eventData[9] & 0x80) >> 7;
            uint8_t event_type = eventData[9] & 0xF;
            switch (event_type)
            {
                case MEMORY_TRAINING_ERR:
                case MEMORY_PMIC_ERR:
                {
                    if (plat == 0)
                    { // Intel
                        errorLog =
                            "GeneralInfo: MEMORY_ECC_ERR(0x" +
                            to_hex_string(generalInfo) + "), " + dimmLocation +
                            ", DIMM Failure Event: " + memoryError[event_type] +
                            ", Major Code: 0x" + to_hex_string(eventData[10]) +
                            ", Minor Code: 0x" + to_hex_string(eventData[11]);
                    }
                    else
                    { // AMD
                        errorLog =
                            "GeneralInfo: MEMORY_ECC_ERR(0x" +
                            to_hex_string(generalInfo) + "), " + dimmLocation +
                            ", DIMM Failure Event: " + memoryError[event_type] +
                            ", Major Code: 0x" + to_hex_string(eventData[10]) +
                            ", Minor Code: 0x" +
                            to_hex_string((eventData[12] << 8) | eventData[11]);
                    }
                    break;
                }
                default:
                    covertToDimmString(dimmInfo.socket, dimmInfo.channel,
                                       dimmInfo.slot, dimm);
                    uint8_t estr_idx = (event_type < memoryError.size())
                                           ? event_type
                                           : (memoryError.size() - 1);
                    errorLog = "GeneralInfo: MEMORY_ECC_ERR(0x" +
                               to_hex_string(generalInfo) + "), " +
                               dimmLocation +
                               ", DIMM Failure Event: " + memoryError[estr_idx];
                    break;
            }
            break;
        }
        case UNIFIED_UPI_ERR:
        {
            uint8_t event_type = eventData[9] & 0xF;
            uint8_t estr_idx = (event_type < upiError.size())
                                   ? event_type
                                   : (upiError.size() - 1);

            switch (event_type)
            {
                case UPI_INIT_ERR:
                {
                    errorLog =
                        "GeneralInfo: UPIErr(0x" + to_hex_string(generalInfo) +
                        "), UPI Port Location: Sled " +
                        std::to_string(dimmInfo.sled) + "/Socket " +
                        std::to_string(dimmInfo.socket) + ", Port " +
                        std::to_string(eventData[3] & 0xF) +
                        ", UPI Failure Event: " + upiError[estr_idx] +
                        ", Major Code: 0x" + to_hex_string(eventData[7]) +
                        ", Minor Code: 0x" + to_hex_string(eventData[8]);
                    break;
                }
                default:
                {
                    errorLog = "GeneralInfo: UPIErr(0x" +
                               to_hex_string(generalInfo) +
                               "), UPI Port Location: Sled " +
                               std::to_string(dimmInfo.sled) + "/Socket " +
                               std::to_string(dimmInfo.socket) + ", Port " +
                               std::to_string(eventData[3] & 0xF) +
                               ", UPI Failure Event: " + upiError[estr_idx];
                    break;
                }
            }
            break;
        }
        case UNIFIED_IIO_ERR:
        {
            uint8_t stack = eventData[6];
            uint8_t sel_error_type = eventData[10];
            uint8_t sel_error_severity = eventData[11];
            uint8_t sel_error_id = eventData[12];

            errorLog = "GeneralInfo: IIOErr(0x" + to_hex_string(generalInfo) +
                       "), IIO Port Location: Sled " +
                       std::to_string(dimmInfo.sled) + "/Socket " +
                       std::to_string(dimmInfo.socket) + ", Stack 0x" +
                       to_hex_string(stack) + ", Error Type: 0x" +
                       to_hex_string(sel_error_type) + ", Error Severity: 0x" +
                       to_hex_string(sel_error_severity) + ", Error ID: 0x" +
                       to_hex_string(sel_error_id);
            break;
        }
        case UNIFIED_RP_PIO_1st:
        case UNIFIED_RP_PIO_2nd:
        {
            auto offset = errorType - UNIFIED_RP_PIO_1st;
            errorLog =
                "GeneralInfo: RP_PIOEvent(0x" + to_hex_string(generalInfo) +
                "), RP_PIO Header Log" + std::to_string(1 + offset * 2) +
                ": 0x" + to_hex_string(eventData[8]) +
                to_hex_string(eventData[7]) + to_hex_string(eventData[6]) +
                to_hex_string(eventData[5]) + ", RP_PIO Header Log" +
                std::to_string(2 + offset * 2) + ": 0x" +
                to_hex_string(eventData[12]) + to_hex_string(eventData[11]) +
                to_hex_string(eventData[10]) + to_hex_string(eventData[9]);
            break;
        }
        case UNIFIED_POST_ERR:
        {
            uint8_t cert_event_idx = (eventData[9] < certEvent.size())
                                         ? eventData[9]
                                         : (certEvent.size() - 1);
            uint8_t fail_type = eventData[10] & 0xF;
            uint8_t err_code = eventData[11];
            uint8_t event_type = eventData[5] & 0xF;
            uint8_t estr_idx = (event_type < postError.size())
                                   ? event_type
                                   : (postError.size() - 1);

            switch (event_type)
            {
                case POST_PXE_BOOT_FAIL:
                case POST_HTTP_BOOT_FAIL:
                {
                    std::string temp_log;
                    if (fail_type == 4 || fail_type == 6)
                    {
                        temp_log = "IPv" + std::to_string(fail_type) + " fail";
                    }
                    else
                    {
                        temp_log = "0x" + to_hex_string(eventData[10]);
                    }
                    errorLog = "GeneralInfo: POST(0x" +
                               to_hex_string(generalInfo) +
                               "), POST Failure Event: " + postError[estr_idx] +
                               ", Fail Type: " + temp_log + ", Error Code: 0x" +
                               to_hex_string(err_code);
                    break;
                }
                case POST_GET_CERT_FAIL:
                {
                    errorLog = "GeneralInfo: POST(0x" +
                               to_hex_string(generalInfo) +
                               "), POST Failure Event: " + postError[estr_idx] +
                               ", Failure Detail: " + certEvent[cert_event_idx];
                    break;
                }
                case POST_AMD_ABL_FAIL:
                {
                    uint16_t abl_err_code = (eventData[12] << 8) |
                                            eventData[11];
                    errorLog =
                        "GeneralInfo: POST(0x" + to_hex_string(generalInfo) +
                        "), POST Failure Event: " + postError[estr_idx] +
                        ", ABL Error Code: 0x" + to_hex_string(abl_err_code);
                    break;
                }
                default:
                {
                    errorLog = "GeneralInfo: POST(0x" +
                               to_hex_string(generalInfo) +
                               "), POST Failure Event: " + postError[estr_idx];
                    break;
                }
            }
            break;
        }
        case UNIFIED_PCIE_EVENT:
        {
            uint8_t event_type = eventData[5] & 0xF;
            uint8_t estr_idx = (event_type < pcieEvent.size())
                                   ? event_type
                                   : (pcieEvent.size() - 1);
            switch (event_type)
            {
                case PCIE_DPC:
                {
                    errorLog =
                        "GeneralInfo: PCIeEvent(0x" +
                        to_hex_string(generalInfo) +
                        "), PCIe Failure Event: " + pcieEvent[estr_idx] +
                        ", Status: 0x" +
                        to_hex_string((eventData[8] << 8) | eventData[7]) +
                        ", Source ID: 0x" +
                        to_hex_string((eventData[10] << 8) | eventData[9]);
                    break;
                }
                default:
                {
                    errorLog = "GeneralInfo: PCIeEvent(0x" +
                               to_hex_string(generalInfo) +
                               "), PCIe Failure Event: " + pcieEvent[estr_idx];
                    break;
                }
            }
            break;
        }
        case UNIFIED_MEM_EVENT:
        {
            // get dimm location data string.
            std::string dimmLocation, dimm;
            getCommonDimmLocation(dimmInfo, dimmLocation, dimm);

            // Event-Type Bit[3:0]
            uint8_t event_type = eventData[9] & 0x0F;
            switch (event_type)
            {
                case MEM_PPR:
                {
                    errorLog = "GeneralInfo: MemEvent(0x" +
                               to_hex_string(generalInfo) + "), " +
                               dimmLocation + ", DIMM Failure Event: " +
                               memoryPprRepairTime[eventData[10] >> 2 & 0x03] +
                               ", " + memoryPprEvent[eventData[10] & 0x03];
                    break;
                }
                case MEM_ADDDC:
                {
                    uint8_t estr_idx = eventData[11] & 0x03;
                    if (estr_idx >= memoryAdddcEvent.size())
                        estr_idx = memoryAdddcEvent.size() - 1;
                    errorLog =
                        "GeneralInfo: MemEvent(0x" +
                        to_hex_string(generalInfo) + "), " + dimmLocation +
                        ", DIMM Failure Event: " + memoryEvent[event_type] +
                        " " + memoryAdddcEvent[estr_idx];
                    break;
                }
                case MEM_NO_DIMM:
                {
                    errorLog =
                        "GeneralInfo: MemEvent(0x" +
                        to_hex_string(generalInfo) +
                        "), DIMM Failure Event: " + memoryEvent[event_type];
                    break;
                }
                default:
                {
                    uint8_t estr_idx = (event_type < memoryEvent.size())
                                           ? event_type
                                           : (memoryEvent.size() - 1);
                    errorLog = "GeneralInfo: MemEvent(0x" +
                               to_hex_string(generalInfo) + "), " +
                               dimmLocation +
                               ", DIMM Failure Event: " + memoryEvent[estr_idx];
                    break;
                }
            }
            break;
        }
        case UNIFIED_UPI_EVENT:
        {
            uint8_t event_type = eventData[9] & 0x0F;
            uint8_t estr_idx = (event_type < upiEvent.size())
                                   ? event_type
                                   : (upiEvent.size() - 1);
            errorLog = "GeneralInfo: UPIEvent(0x" + to_hex_string(generalInfo) +
                       "), UPI Port Location: Sled " +
                       std::to_string(dimmInfo.sled) + "/Socket " +
                       std::to_string(dimmInfo.socket) + ", Port " +
                       std::to_string(eventData[6] & 0xF) +
                       ", UPI Failure Event: " + upiEvent[estr_idx];
            break;
        }
        case UNIFIED_BOOT_GUARD:
        {
            errorLog = "GeneralInfo: Boot Guard ACM Failure Events(0x" +
                       to_hex_string(generalInfo) + "), Error Class(0x" +
                       to_hex_string(eventData[9]) + "), Major Error Code(0x" +
                       to_hex_string(eventData[10]) + "), Minor Error Code(0x" +
                       to_hex_string(eventData[11]) + ")";
            break;
        }
        case UNIFIED_PPR_EVENT:
        {
            uint8_t event_type = eventData[5] & 0x0F;
            errorLog =
                "GeneralInfo: PPR Events(0x" + to_hex_string(generalInfo) +
                "), " + pprEvent[event_type] + ". DIMM Info: (" +
                to_hex_string(eventData[6]) + to_hex_string(eventData[7]) +
                to_hex_string(eventData[8]) + to_hex_string(eventData[9]) +
                to_hex_string(eventData[10]) + to_hex_string(eventData[11]) +
                to_hex_string(eventData[12]) + ")";
            break;
        }
        case UNIFIED_CXL_MEM_ERR:
        {
            uint8_t event_type = eventData[9] & 0xF;
            errorLog = "GeneralInfo: CXL Memory Error(0x" +
                       to_hex_string(generalInfo) + "), Bus " +
                       to_hex_string(eventData[6]) + "/Dev " +
                       to_hex_string(eventData[7] >> 4) + "/Fun " +
                       to_hex_string(eventData[7] & 0xF) +
                       ", Controller ID(0x" + to_hex_string(eventData[10]) +
                       "), DIMM Failure Event: " + memoryError[event_type];
            break;
        }
        default:
        {
            errorLog =
                "Undefined Error Type(0x" + to_hex_string(errorType) +
                "), Raw: " + to_hex_string(eventData[0]) +
                to_hex_string(eventData[1]) + to_hex_string(eventData[2]) +
                to_hex_string(eventData[3]) + to_hex_string(eventData[4]) +
                to_hex_string(eventData[5]) + to_hex_string(eventData[6]) +
                to_hex_string(eventData[7]) + to_hex_string(eventData[8]) +
                to_hex_string(eventData[9]) + to_hex_string(eventData[10]) +
                to_hex_string(eventData[11]) + to_hex_string(eventData[12]);
            break;
        }
    }

    auto slotNumber = getSlotNumber(tid, configurations);
    std::string message = "Host " + slotNumber + ": " + errorLog;
    lg2::error("BIOS_IPMI_SEL: {ERROR}", "ERROR",
               message);                       // Create log in journal
    pldm::utils::reportError(message.c_str()); // Create log on Dbus
    return 0;
}

} // namespace oem_meta
} // namespace platform_mc
} // namespace pldm
