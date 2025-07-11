#include "oem_event_manager.hpp"

#include "common/utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Slot/client.hpp>

#include <cstdint>
#include <iostream>
#include <sstream>

namespace pldm
{
namespace oem_meta
{

int OemEventManager::processOemMetaEvent(
    pldm_tid_t tid, const uint8_t* eventData,
    [[maybe_unused]] size_t eventDataSize,
    const std::map<std::string, pldm::utils::oem_meta::MctpEndpoint>&
        configurations) const
{
    RecordType recordType = static_cast<RecordType>(eventData[0]);
    std::string errorLog;
    switch (recordType)
    {
        case RecordType::SYSTEM_EVENT_RECORD:
        {
            handleSystemEvent(eventData, errorLog);
            break;
        }
        case RecordType::UNIFIED_BIOS_SEL:
        {
            handleUnifiedMemoryEvent(eventData, errorLog);
            break;
        }
        default:
        {
            return PLDM_ERROR;
        }
    }
    auto slotNumber = getSlotNumberString(tid, configurations);
    std::string message =
        "SEL Entry: Host: " + slotNumber + ", Record: " + errorLog;
    lg2::error("{ERROR}", "ERROR", message);   // Create log in journal
    pldm::utils::reportError(message.c_str()); // Create log on Dbus
    return 0;
}

int OemEventManager::handleOemEvent(
    const pldm_msg* request, size_t payloadLength, uint8_t /* formatVersion */,
    uint8_t tid, size_t eventDataOffset)
{
    auto eventData =
        reinterpret_cast<const uint8_t*>(request->payload) + eventDataOffset;
    auto eventDataSize = payloadLength - eventDataOffset;
    const std::map<std::string, pldm::utils::oem_meta::MctpEndpoint>&
        configurations = configurationDiscovery->getConfigurations();
    if (!pldm::utils::oem_meta::checkMetaIana(tid, configurations))
    {
        lg2::error("Recieve OEM Meta event from not Meta specific device");
        return PLDM_ERROR;
    }
    if (!processOemMetaEvent(tid, eventData, eventDataSize, configurations))
        return PLDM_ERROR;

    return PLDM_SUCCESS;
}

std::string OemEventManager::getSlotNumberString(
    pldm_tid_t tid,
    const std::map<std::string, pldm::utils::oem_meta::MctpEndpoint>&
        configurations) const
{
    using DecoratorSlot =
        sdbusplus::client::xyz::openbmc_project::inventory::decorator::Slot<>;

    std::string slotNumber = "Unknown";
    for (const auto& [configDbusPath, mctpEndpoint] : configurations)
    {
        if (mctpEndpoint.EndpointId == tid)
        {
            std::string endpointDbusPath;
            try
            {
                auto response = pldm::utils::DBusHandler().getAncestors(
                    configDbusPath.c_str(), {DecoratorSlot::interface});
                if (response.size() != 1)
                {
                    lg2::error(
                        "Only Board layer should have Decorator.Slot interface, got {SIZE} Dbus Object(s) have interface Decorator.Slot}",
                        "SIZE", response.size());
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
                        endpointDbusPath.c_str(), "SlotNumber",
                        DecoratorSlot::interface);
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

void OemEventManager::handleSystemEvent(const uint8_t* eventData,
                                        std::string& errorLog) const
{
    errorLog = "Standard (0x02), ";
    // CXL SEL
    if (eventData[8] == 0xC1)
    {
        errorLog += "CXL Error(0xC1), ";
        if (eventData[9] == 0x1)
        {
            errorLog += ", Protocol Error(0x1), ";
        }
        else
        {
            errorLog += ", Component Error(0x2), ";
        }

        errorLog += "Severity: ";
        errorLog += (eventData[11] & 0x01) ? "Uncorrectable" : "Correctable";
        errorLog += ", Detected By: ";
        errorLog += (eventData[11] & 0x02) ? "CXL 1.1 Host Downstream Port"
                                           : "CXL 1.1 Device";

        uint8_t errorType = (eventData[11] >> 2) & 0x03;

        errorLog += ", Error Type: " + std::string(cxlError[errorType]) +
                    ", Bus " + to_hex_string(eventData[12]) + "/Dev " +
                    to_hex_string(eventData[13] >> 3) + "/Fun " +
                    to_hex_string(eventData[13] & 0x7);
    }
    // CXL SEL 2.0, used only for device type = root port
    else if (eventData[8] == 0xC2)
    {
        errorLog += "CXL Error(0xC2), ";
        if (eventData[9] == 0x1)
        {
            errorLog += ", Protocol Error(0x1), ";
        }
        else
        {
            errorLog += ", Component Error(0x2), ";
        }

        errorLog += "Severity: ";
        errorLog += (eventData[11] & 0x01) ? "Uncorrectable" : "Correctable";
        // root port only
        errorLog += ", Detected By: CXL Root Port";

        uint8_t errorType = (eventData[11] >> 2) & 0x03;

        errorLog += ", Error Type: " + std::string(cxlError[errorType]) +
                    ", Bus " + to_hex_string(eventData[12]) + "/Dev " +
                    to_hex_string(eventData[13] >> 3) + "/Fun " +
                    to_hex_string(eventData[13] & 0x7);
    }
    // Processer ERROR
    else if (eventData[8] == 0x07)
    {
        if (eventData[9] == 0x40)
        {
            errorLog += "MACHINE_CHK_ERR (" + to_hex_string(eventData[9]) +
                        "), ";
            errorLog += "Event Data: (" + to_hex_string(eventData[11]) +
                        to_hex_string(eventData[12]) +
                        to_hex_string(eventData[13]) + ") ";

            uint8_t severity = eventData[11] & 0x0F;
            switch (severity)
            {
                case 0x0B:
                    errorLog += "Uncorrected System Fatal Error, ";
                    break;
                case 0x0C:
                    errorLog += "Correctable System Error, ";
                    break;
                default:
                    errorLog += "Unknown Error, ";
                    break;
            }

            if (eventData[12] == 0x1D)
            {
                errorLog += "Bank Number " + std::to_string(eventData[12]) +
                            " (SMU/MPDMA), ";
            }

            errorLog += "CPU " + std::to_string(eventData[13] >> 5) + ", CCD " +
                        std::to_string(eventData[13] & 0x1F);
        }
    }
    // POST ERROR
    else if (eventData[9] == 0x2B)
    {
        errorLog += "POST_ERROR:";
        if ((eventData[11] & 0x0F) == 0x0)
            errorLog += ", System Firmware Error";

        if (((eventData[11] >> 6) & 0x03) == 0x2)
        {
            errorLog += ", OEM Post Code 0x" + to_hex_string(eventData[13]) +
                        to_hex_string(eventData[12]);
        }

        switch ((eventData[13] << 8) | eventData[12])
        {
            case 0xD9:
                errorLog +=
                    ", Error loading Boot Option (Load image returned error)";
                break;
            default:
                break;
        }
    }
    else
    { // Not supported
        errorLog +=
            "Raw: " + to_hex_string(eventData[1]) +
            to_hex_string(eventData[2]) + to_hex_string(eventData[3]) +
            to_hex_string(eventData[4]) + to_hex_string(eventData[5]) +
            to_hex_string(eventData[6]) + to_hex_string(eventData[7]) +
            to_hex_string(eventData[8]) + to_hex_string(eventData[9]) +
            to_hex_string(eventData[10]) + to_hex_string(eventData[11]) +
            to_hex_string(eventData[12]) + to_hex_string(eventData[13]);
    }
}

void OemEventManager::handleUnifiedMemoryEvent(const uint8_t* eventData,
                                               std::string& errorLog) const
{
    errorLog = "Facebook Unified SEL (0xFB), ";

    DimmInfo dimmInfo = {
        static_cast<uint8_t>((eventData[6] >> 4) & 0x03), // Sled
        static_cast<uint8_t>(eventData[6] & 0x0F),        // Socket
        static_cast<uint8_t>(eventData[7]),               // Channel
        static_cast<uint8_t>(eventData[8])                // Slot
    };

    uint8_t generalInfo = eventData[1];
    UnifiedError errorType = static_cast<UnifiedError>(generalInfo & 0xF);
    switch (errorType)
    {
        case UnifiedError::UNIFIED_PCIE_ERR:
        {
            uint8_t plat = (generalInfo & 0x10) >> 4;
            if (plat == 0)
            { // x86
                errorLog +=
                    "GeneralInfo: x86/PCIeErr(0x" + to_hex_string(generalInfo) +
                    "), Bus " + to_hex_string(eventData[9]) + "/Dev " +
                    to_hex_string(eventData[8] >> 3) + "/Fun " +
                    to_hex_string(eventData[8] & 0x7) + ", ErrID2: 0x" +
                    to_hex_string(eventData[12]) + ", ErrID1: 0x" +
                    to_hex_string(eventData[13]);
            }
            else
            {
                errorLog +=
                    "GeneralInfo: ARM/PCIeErr(0x" + to_hex_string(generalInfo) +
                    "), Aux. Info: 0x" +
                    to_hex_string((eventData[7] << 8) | eventData[6], 4) +
                    ", Bus " + to_hex_string(eventData[9]) + "/Dev " +
                    to_hex_string(eventData[8] >> 3) + "/Fun " +
                    to_hex_string(eventData[8] & 0x7) + ", ErrID2: 0x" +
                    to_hex_string(eventData[12]) + ", ErrID1: 0x" +
                    to_hex_string(eventData[13]);
            }
            break;
        }
        case UnifiedError::UNIFIED_MEM_ERR:
        {
            handleMemoryError(eventData, errorLog, dimmInfo, generalInfo);
            break;
        }
        case UnifiedError::UNIFIED_UPI_ERR:
        {
            UpiError eventType = static_cast<UpiError>(eventData[10] & 0xF);
            uint8_t estrIdx =
                (static_cast<uint8_t>(eventType) < upiError.size())
                    ? static_cast<uint8_t>(eventType)
                    : (upiError.size() - 1);

            switch (eventType)
            {
                case UpiError::UPI_INIT_ERR:
                {
                    errorLog +=
                        "GeneralInfo: UPIErr(0x" + to_hex_string(generalInfo) +
                        "), UPI Port Location: Sled " +
                        std::to_string(dimmInfo.sled) + "/Socket " +
                        std::to_string(dimmInfo.socket) + ", Port " +
                        std::to_string(eventData[4] & 0xF) +
                        ", UPI Failure Event: " + upiError[estrIdx] +
                        ", Major Code: 0x" + to_hex_string(eventData[8]) +
                        ", Minor Code: 0x" + to_hex_string(eventData[9]);
                    break;
                }
                default:
                {
                    errorLog +=
                        "GeneralInfo: UPIErr(0x" + to_hex_string(generalInfo) +
                        "), UPI Port Location: Sled " +
                        std::to_string(dimmInfo.sled) + "/Socket " +
                        std::to_string(dimmInfo.socket) + ", Port " +
                        std::to_string(eventData[4] & 0xF) +
                        ", UPI Failure Event: " + upiError[estrIdx];
                    break;
                }
            }
            break;
        }
        case UnifiedError::UNIFIED_IIO_ERR:
        {
            uint8_t stack = eventData[7];
            uint8_t selErrorType = eventData[11];
            uint8_t selErrorSeverity = eventData[12];
            uint8_t selErrorId = eventData[13];

            errorLog +=
                "GeneralInfo: IIOErr(0x" + to_hex_string(generalInfo) +
                "), IIO Port Location: Sled " + std::to_string(dimmInfo.sled) +
                "/Socket " + std::to_string(dimmInfo.socket) + ", Stack 0x" +
                to_hex_string(stack) + ", Error Type: 0x" +
                to_hex_string(selErrorType) + ", Error Severity: 0x" +
                to_hex_string(selErrorSeverity) + ", Error ID: 0x" +
                to_hex_string(selErrorId);
            break;
        }
        case UnifiedError::UNIFIED_MCA_ERR:
        {
            uint8_t mcaSeverity = ((eventData[6] >> 4) & 0x07);
            uint8_t cpuNumber = (eventData[7] >> 5) & 0x07;
            uint8_t coreNumber = eventData[7] & 0x1F;
            uint8_t machineCheckBankNumber = eventData[8];
            uint32_t errorInfo = (static_cast<uint32_t>(eventData[9]) << 16) |
                                 (static_cast<uint32_t>(eventData[10]) << 8) |
                                 static_cast<uint32_t>(eventData[11]);
            uint8_t errorCode = eventData[12];
            uint8_t errorStatus = eventData[13];

            errorLog +=
                "GeneralInfo: MCAErr(0x" + to_hex_string(generalInfo) +
                "), MCA Severity: " + errorSeverityDetail[mcaSeverity] +
                ", CPU Number: " + std::to_string(cpuNumber) +
                ", Core Number: " + std::to_string(coreNumber) +
                ", Machine Check Bank: " +
                machineCheckBank[machineCheckBankNumber] + ", Error Info: 0x" +
                to_hex_string(errorInfo, 6) + ", Error Code: 0x" +
                to_hex_string(errorCode) + ", Error Status: 0x" +
                to_hex_string(errorStatus);
            break;
        }
        case UnifiedError::UNIFIED_MCA_ERR_EXT:
        {
            uint8_t mcaSeverity = ((eventData[6] >> 4) & 0x07);
            uint8_t cpuNumber = (eventData[7] >> 5) & 0x07;
            uint8_t coreNumber = eventData[7] & 0x1F;
            uint8_t machineCheckBankNumber = eventData[8];
            uint16_t errorCode = (eventData[9] << 8) | eventData[10];

            errorLog +=
                "GeneralInfo: MCAErrExt(0x" + to_hex_string(generalInfo) +
                "), MCA Severity: " + errorSeverityDetail[mcaSeverity] +
                ", CPU Number: " + std::to_string(cpuNumber) +
                ", Core Number: " + std::to_string(coreNumber) +
                ", Machine Check Bank: " +
                machineCheckBank[machineCheckBankNumber] + ", Error Code: 0x" +
                to_hex_string(errorCode, 4);
            break;
        }
        case UnifiedError::UNIFIED_RP_PIO_1st:
        case UnifiedError::UNIFIED_RP_PIO_2nd:
        {
            auto offset =
                static_cast<uint8_t>(errorType) -
                static_cast<uint8_t>(UnifiedError::UNIFIED_RP_PIO_1st);
            errorLog +=
                "GeneralInfo: RP_PIOEvent(0x" + to_hex_string(generalInfo) +
                "), RP_PIO Header Log" + std::to_string(1 + offset * 2) +
                ": 0x" + to_hex_string(eventData[9]) +
                to_hex_string(eventData[8]) + to_hex_string(eventData[7]) +
                to_hex_string(eventData[6]) + ", RP_PIO Header Log" +
                std::to_string(2 + offset * 2) + ": 0x" +
                to_hex_string(eventData[13]) + to_hex_string(eventData[12]) +
                to_hex_string(eventData[11]) + to_hex_string(eventData[10]);
            break;
        }
        case UnifiedError::UNIFIED_POST_EVENT:
        {
            handleSystemPostEvent(eventData, errorLog, generalInfo);
            break;
        }
        case UnifiedError::UNIFIED_PCIE_EVENT:
        {
            PcieEvent eventType = static_cast<PcieEvent>(eventData[6] & 0xF);
            uint8_t estrIdx =
                (static_cast<uint8_t>(eventType) < pcieEvent.size())
                    ? static_cast<uint8_t>(eventType)
                    : (pcieEvent.size() - 1);
            switch (eventType)
            {
                case PcieEvent::PCIE_DPC:
                {
                    errorLog +=
                        "GeneralInfo: PCIeEvent(0x" +
                        to_hex_string(generalInfo) +
                        "), PCIe Failure Event: " + pcieEvent[estrIdx] +
                        ", Status: 0x" +
                        to_hex_string((eventData[9] << 8) | eventData[8], 4) +
                        ", Source ID: 0x" +
                        to_hex_string((eventData[11] << 8) | eventData[10], 4);
                    break;
                }
                default:
                {
                    errorLog += "GeneralInfo: PCIeEvent(0x" +
                                to_hex_string(generalInfo) +
                                "), PCIe Failure Event: " + pcieEvent[estrIdx];
                    break;
                }
            }
            break;
        }
        case UnifiedError::UNIFIED_MEM_EVENT:
        {
            // get dimm location data string.
            std::string dimmLocation, dimm;
            getCommonDimmLocation(dimmInfo, dimmLocation, dimm);

            // Event-Type Bit[3:0]
            MemoryEvent eventType =
                static_cast<MemoryEvent>(eventData[10] & 0x0F);
            switch (eventType)
            {
                case MemoryEvent::MEM_PPR:
                {
                    errorLog += "GeneralInfo: MemEvent(0x" +
                                to_hex_string(generalInfo) + "), " +
                                dimmLocation + ", DIMM Failure Event: " +
                                memoryPprRepairTime[eventData[11] >> 2 & 0x03] +
                                ", " + memoryPprEvent[eventData[11] & 0x03];
                    break;
                }
                case MemoryEvent::MEM_ADDDC:
                {
                    uint8_t estrIdx = eventData[12] & 0x03;
                    if (estrIdx >= memoryAdddcEvent.size())
                        estrIdx = memoryAdddcEvent.size() - 1;
                    errorLog += "GeneralInfo: MemEvent(0x" +
                                to_hex_string(generalInfo) + "), " +
                                dimmLocation + ", DIMM Failure Event: " +
                                memoryEvent[static_cast<uint8_t>(eventType)] +
                                " " + memoryAdddcEvent[estrIdx];
                    break;
                }
                case MemoryEvent::MEM_NO_DIMM:
                {
                    errorLog += "GeneralInfo: MemEvent(0x" +
                                to_hex_string(generalInfo) +
                                "), DIMM Failure Event: " +
                                memoryEvent[static_cast<uint8_t>(eventType)];
                    break;
                }
                case MemoryEvent::MEM_CXL_POST_PPR:
                {
                    errorLog +=
                        "GeneralInfo: MemEvent(0x" +
                        to_hex_string(generalInfo) + "), Bus " +
                        to_hex_string(eventData[12]) + "/Dev " +
                        to_hex_string(eventData[13] >> 3) + "/Fun " +
                        to_hex_string(eventData[13] & 0x7) +
                        ", DIMM Failure Event: " +
                        memoryEvent[static_cast<uint8_t>(eventType)] + ", " +
                        memoryCXLPostPprEvent[eventData[11] >> 7 & 0x1] + ", " +
                        memoryCxlEvent[eventData[11] & 0x03];
                    break;
                }
                default:
                {
                    uint8_t estrIdx =
                        (static_cast<uint8_t>(eventType) < memoryEvent.size())
                            ? static_cast<uint8_t>(eventType)
                            : (memoryEvent.size() - 1);
                    errorLog += "GeneralInfo: MemEvent(0x" +
                                to_hex_string(generalInfo) + "), " +
                                dimmLocation +
                                ", DIMM Failure Event: " + memoryEvent[estrIdx];
                    break;
                }
            }
            break;
        }
        case UnifiedError::UNIFIED_UPI_EVENT:
        {
            uint8_t eventType = eventData[10] & 0x0F;
            uint8_t estrIdx = (eventType < upiEvent.size())
                                  ? eventType
                                  : (upiEvent.size() - 1);
            errorLog +=
                "GeneralInfo: UPIEvent(0x" + to_hex_string(generalInfo) +
                "), UPI Port Location: Sled " + std::to_string(dimmInfo.sled) +
                "/Socket " + std::to_string(dimmInfo.socket) + ", Port " +
                std::to_string(eventData[7] & 0xF) +
                ", UPI Failure Event: " + upiEvent[estrIdx];
            break;
        }
        case UnifiedError::UNIFIED_BOOT_GUARD:
        {
            errorLog +=
                "GeneralInfo: Boot Guard ACM Failure Events(0x" +
                to_hex_string(generalInfo) + "), Error Class(0x" +
                to_hex_string(eventData[10]) + "), Major Error Code(0x" +
                to_hex_string(eventData[11]) + "), Minor Error Code(0x" +
                to_hex_string(eventData[12]) + ")";
            break;
        }
        case UnifiedError::UNIFIED_PPR_EVENT:
        {
            uint8_t eventType = eventData[6] & 0x0F;
            errorLog +=
                "GeneralInfo: PPR Events(0x" + to_hex_string(generalInfo) +
                "), " + pprEvent[eventType] + ". DIMM Info: (" +
                to_hex_string(eventData[7]) + to_hex_string(eventData[8]) +
                to_hex_string(eventData[9]) + to_hex_string(eventData[10]) +
                to_hex_string(eventData[11]) + to_hex_string(eventData[12]) +
                to_hex_string(eventData[13]) + ")";
            break;
        }
        case UnifiedError::UNIFIED_CXL_MEM_ERR:
        {
            uint8_t eventType = eventData[10] & 0xF;
            errorLog += "GeneralInfo: CXL Memory Error(0x" +
                        to_hex_string(generalInfo) + "), Bus " +
                        to_hex_string(eventData[7]) + "/Dev " +
                        to_hex_string(eventData[8] >> 3) + "/Fun " +
                        to_hex_string(eventData[8] & 0x7) +
                        ", DIMM Failure Event: " + memoryError[eventType] +
                        ", " + memoryCxlEvent[eventData[11]];
            break;
        }
        default:
        {
            errorLog +=
                "Undefined Error Type(0x" +
                to_hex_string(static_cast<uint8_t>(errorType)) +
                "), Raw: " + to_hex_string(eventData[1]) +
                to_hex_string(eventData[2]) + to_hex_string(eventData[3]) +
                to_hex_string(eventData[4]) + to_hex_string(eventData[5]) +
                to_hex_string(eventData[6]) + to_hex_string(eventData[7]) +
                to_hex_string(eventData[8]) + to_hex_string(eventData[9]) +
                to_hex_string(eventData[10]) + to_hex_string(eventData[11]) +
                to_hex_string(eventData[12]) + to_hex_string(eventData[13]);
            break;
        }
    }
}

void OemEventManager::handleMemoryError(
    const uint8_t* eventData, std::string& errorLog, const DimmInfo& dimmInfo,
    uint8_t generalInfo) const
{
    std::string dimmLocation, dimm;
    getCommonDimmLocation(dimmInfo, dimmLocation, dimm);
    uint8_t plat = (eventData[10] & 0x80) >> 7;
    MemoryError eventType = static_cast<MemoryError>(eventData[10] & 0xF);
    switch (eventType)
    {
        case MemoryError::MEMORY_TRAINING_ERR:
        case MemoryError::MEMORY_PMIC_ERR:
        {
            if (plat == 0)
            { // Intel
                errorLog += "GeneralInfo: MEMORY_ECC_ERR(0x" +
                            to_hex_string(generalInfo) + "), " + dimmLocation +
                            ", DIMM Failure Event: " +
                            memoryError[static_cast<uint8_t>(eventType)] +
                            ", Major Code: 0x" + to_hex_string(eventData[11]) +
                            ", Minor Code: 0x" + to_hex_string(eventData[12]);
            }
            else
            { // AMD
                errorLog +=
                    "GeneralInfo: MEMORY_ECC_ERR(0x" +
                    to_hex_string(generalInfo) + "), " + dimmLocation +
                    ", DIMM Failure Event: " +
                    memoryError[static_cast<uint8_t>(eventType)] +
                    ", Major Code: 0x" + to_hex_string(eventData[11]) +
                    ", Minor Code: 0x" +
                    to_hex_string((eventData[13] << 8) | eventData[12], 4);
            }
            break;
        }
        case MemoryError::MEMORY_CXL_TRAINNING_ERR:
        {
            errorLog +=
                "GeneralInfo: MEMORY_CXL_TRAINNING_ERR(0x" +
                to_hex_string(generalInfo) + "), " + dimmLocation +
                ", DIMM Failure Event: " +
                memoryError[static_cast<uint8_t>(eventType)] + ", " +
                memoryCxlEvent[static_cast<uint8_t>(eventData[11] & 0x7)];
            break;
        }
        default:
            covertToDimmString(dimmInfo.socket, dimmInfo.channel, dimmInfo.slot,
                               dimm);
            uint8_t estrIdx =
                (static_cast<uint8_t>(eventType) < memoryError.size())
                    ? static_cast<uint8_t>(eventType)
                    : (memoryError.size() - 1);
            errorLog += "GeneralInfo: MEMORY_ECC_ERR(0x" +
                        to_hex_string(generalInfo) + "), " + dimmLocation +
                        ", DIMM Failure Event: " + memoryError[estrIdx];
            break;
    }
}

void OemEventManager::handleSystemPostEvent(
    const uint8_t* eventData, std::string& errorLog, uint8_t generalInfo) const
{
    uint8_t certEventIdx = (eventData[10] < certEvent.size())
                               ? eventData[10]
                               : (certEvent.size() - 1);
    uint8_t failType = eventData[11] & 0xF;
    uint8_t errCode = eventData[12];
    PostError eventType = static_cast<PostError>(eventData[6] & 0xF);
    uint8_t estrIdx = (static_cast<uint8_t>(eventType) < postError.size())
                          ? static_cast<uint8_t>(eventType)
                          : (postError.size() - 1);

    switch (eventType)
    {
        case PostError::POST_PXE_BOOT_FAIL:
        case PostError::POST_HTTP_BOOT_FAIL:
        {
            std::string tempLog;
            if (failType == 4 || failType == 6)
            {
                tempLog = "IPv" + std::to_string(failType) + " fail";
            }
            else
            {
                tempLog = "0x" + to_hex_string(eventData[11]);
            }
            errorLog += "GeneralInfo: POST(0x" + to_hex_string(generalInfo) +
                        "), POST Failure Event: " + postError[estrIdx] +
                        ", Fail Type: " + tempLog + ", Error Code: 0x" +
                        to_hex_string(errCode);
            break;
        }
        case PostError::POST_GET_CERT_FAIL:
        {
            errorLog += "GeneralInfo: POST(0x" + to_hex_string(generalInfo) +
                        "), POST Failure Event: " + postError[estrIdx] +
                        ", Failure Detail: " + certEvent[certEventIdx];
            break;
        }
        case PostError::POST_AMD_ABL_FAIL:
        {
            uint16_t ablErrCode = (eventData[13] << 8) | eventData[12];
            errorLog += "GeneralInfo: POST(0x" + to_hex_string(generalInfo) +
                        "), POST Failure Event: " + postError[estrIdx] +
                        ", ABL Error Code: 0x" + to_hex_string(ablErrCode, 4);
            break;
        }
        case PostError::POST_BOOT_DRIVE_FAIL:
        case PostError::POST_DATA_DRIVE_FAIL:
        case PostError::POST_CXL_NOT_READY:
        case PostError::POST_CXL_ERR_RECORD_CLEARED_BY_BIOS:
        {
            errorLog += "GeneralInfo: POST(0x" + to_hex_string(generalInfo) +
                        "), Bus " + to_hex_string(eventData[7]) + "/Dev " +
                        to_hex_string(eventData[8] >> 3) + "/Fun " +
                        to_hex_string(eventData[8] & 0x7) +
                        ", POST Failure Event: " + postError[estrIdx];
            break;
        }
        default:
        {
            errorLog += "GeneralInfo: POST(0x" + to_hex_string(generalInfo) +
                        "), POST Failure Event: " + postError[estrIdx];
            break;
        }
    }
}

} // namespace oem_meta
} // namespace pldm
