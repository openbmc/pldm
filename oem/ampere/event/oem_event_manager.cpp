#include "oem_event_manager.hpp"

#include "requester/handler.hpp"
#include "requester/request.hpp"

#include <config.h>
#include <libpldm/pldm.h>
#include <libpldm/utils.h>
#include <systemd/sd-journal.h>

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>

namespace pldm
{
namespace oem_ampere
{
namespace boot_stage = boot::stage;
namespace ddr_status = ddr::status;
namespace dimm_status = dimm::status;
namespace dimm_syndrome = dimm::training_failure::dimm_syndrome;
namespace phy_syndrome = dimm::training_failure::phy_syndrome;
namespace training_failure = dimm::training_failure;

constexpr const char* ampereEventRegistry = "OpenBMC.0.1.AmpereEvent.OK";
constexpr const char* ampereWarningRegistry =
    "OpenBMC.0.1.AmpereWarning.Warning";
constexpr const char* ampereCriticalRegistry =
    "OpenBMC.0.1.AmpereCritical.Critical";
constexpr const char* BIOSFWPanicRegistry =
    "OpenBMC.0.1.BIOSFirmwarePanicReason.Warning";
constexpr auto maxDIMMIdxBitNum = 24;
constexpr auto maxDIMMInstantNum = 24;

/*
    An array of possible boot status of a boot stage.
    The index maps with byte 0 of boot code.
*/
std::array<std::string, 3> bootStatMsg = {" booting", " completed", " failed"};

/*
    An array of possible boot status of DDR training stage.
    The index maps with byte 0 of boot code.
*/
std::array<std::string, 3> ddrTrainingMsg = {
    " progress started", " in-progress", " progress completed"};

/*
    A map between PMIC status and logging strings.
*/
std::array<std::string, 8> pmicTempAlertMsg = {
    "Below 85°C", "85°C",  "95°C",  "105°C",
    "115°C",      "125°C", "135°C", "Equal or greater than 140°C"};

/*
    In Ampere systems, BMC only directly communicates with MCTP/PLDM SoC
    EPs through SMBus and PCIe. When host boots up, SMBUS interface
    comes up first. In this interface, BMC is bus owner.

    mctpd will set the EID 0x14 for S0 and 0x16 for S1 (if available).
    pldmd will always use TID 1 for S0 and TID 2 for S1 (if available).
*/
EventToMsgMap_t tidToSocketNameMap = {{1, "SOCKET 0"}, {2, "SOCKET 1"}};

/*
    A map between sensor IDs and their names in string.
    Using pldm::oem::sensor_ids
*/
EventToMsgMap_t sensorIdToStrMap = {
    {DDR_STATUS, "DDR_STATUS"},
    {PCP_VR_STATE, "PCP_VR_STATE"},
    {SOC_VR_STATE, "SOC_VR_STATE"},
    {DPHY_VR1_STATE, "DPHY_VR1_STATE"},
    {DPHY_VR2_STATE, "DPHY_VR2_STATE"},
    {D2D_VR_STATE, "D2D_VR_STATE"},
    {IOC_VR1_STATE, "IOC_VR1_STATE"},
    {IOC_VR2_STATE, "IOC_VR2_STATE"},
    {PCI_D_VR_STATE, "PCI_D_VR_STATE"},
    {PCI_A_VR_STATE, "PCI_A_VR_STATE"},
    {PCIE_HOT_PLUG, "PCIE_HOT_PLUG"},
    {BOOT_OVERALL, "BOOT_OVERALL"},
    {SOC_HEALTH_AVAILABILITY, "SOC_HEALTH_AVAILABILITY"}};

/*
    A map between the boot stages and logging strings.
    Using pldm::oem::boot::stage::boot_stage
*/
EventToMsgMap_t bootStageToMsgMap = {
    {boot_stage::SECPRO, "SECpro"},
    {boot_stage::MPRO, "Mpro"},
    {boot_stage::ATF_BL1, "ATF BL1"},
    {boot_stage::ATF_BL2, "ATF BL2"},
    {boot_stage::DDR_INITIALIZATION, "DDR initialization"},
    {boot_stage::DDR_TRAINING, "DDR training"},
    {boot_stage::S0_DDR_TRAINING_FAILURE, "DDR training failure"},
    {boot_stage::ATF_BL31, "ATF BL31"},
    {boot_stage::ATF_BL32, "ATF BL32"},
    {boot_stage::S1_DDR_TRAINING_FAILURE, "DDR training failure"},
    {boot_stage::UEFI_STATUS_CLASS_CODE_MIN,
     "ATF BL33 (UEFI) booting status = "}};

/*
    A map between DDR status and logging strings.
    Using pldm::oem::ddr::status::ddr_status
*/
EventToMsgMap_t ddrStatusToMsgMap = {
    {ddr_status::NO_SYSTEM_LEVEL_ERROR, "has no system level error"},
    {ddr_status::ECC_INITIALIZATION_FAILURE, "has ECC initialization failure"},
    {ddr_status::CONFIGURATION_FAILURE, "has configuration failure at DIMMs:"},
    {ddr_status::TRAINING_FAILURE, "has training failure at DIMMs:"},
    {ddr_status::OTHER_FAILURE, "has other failure"},
    {ddr_status::BOOT_FAILURE_NO_VALID_CONFIG,
     "has boot failure due to no configuration"},
    {ddr_status::FAILSAFE_ACTIVATED_NEXT_BOOT_SUCCESS,
     "failsafe activated but boot success with the next valid configuration"}};

/*
    A map between DIMM status and logging strings.
    Using pldm::oem::dimm::status::dimm_status
*/
EventToMsgMap_t dimmStatusToMsgMap = {
    {dimm_status::INSTALLED_NO_ERROR, "is installed and no error"},
    {dimm_status::NOT_INSTALLED, "is not installed"},
    {dimm_status::OTHER_FAILURE, "has other failure"},
    {dimm_status::INSTALLED_BUT_DISABLED, "is installed but disabled"},
    {dimm_status::TRAINING_FAILURE, "has training failure; "},
    {dimm_status::PMIC_TEMP_ALERT, "has PMIC temperature alert"}};

/*
    A map between PHY training failure syndrome and logging strings.
    Using
   pldm::oem::dimm::training_faillure::phy_syndrome::phy_training_failure_syndrome
*/
EventToMsgMap_t phyTrainingFailureSyndromeToMsgMap = {
    {phy_syndrome::NA, "(N/A)"},
    {phy_syndrome::PHY_TRAINING_SETUP_FAILURE, "(PHY training setup failure)"},
    {phy_syndrome::CA_LEVELING, "(CA leveling)"},
    {phy_syndrome::PHY_WRITE_LEVEL_FAILURE,
     "(PHY write level failure - see syndrome 1)"},
    {phy_syndrome::PHY_READ_GATE_LEVELING_FAILURE,
     "(PHY read gate leveling failure)"},
    {phy_syndrome::PHY_READ_LEVEL_FAILURE, "(PHY read level failure)"},
    {phy_syndrome::WRITE_DQ_LEVELING, "(Write DQ leveling)"},
    {phy_syndrome::PHY_SW_TRAINING_FAILURE, "(PHY SW training failure)"}};

/*
    A map between DIMM training failure syndrome and logging strings.
    Using
   pldm::oem::dimm::training_faillure::dimm_syndrome::dimm_training_failure_syndrome
*/
EventToMsgMap_t dimmTrainingFailureSyndromeToMsgMap = {
    {dimm_syndrome::NA, "(N/A)"},
    {dimm_syndrome::DRAM_VREFDQ_TRAINING_FAILURE,
     "(DRAM VREFDQ training failure)"},
    {dimm_syndrome::LRDIMM_DB_TRAINING_FAILURE, "(LRDIMM DB training failure)"},
    {dimm_syndrome::LRDRIMM_DB_SW_TRAINING_FAILURE,
     "(LRDRIMM DB SW training failure)"}};

/*
    A map between DIMM training failure type and a pair of <logging strings -
   syndrome map>. Using
   pldm::oem::dimm::training_faillure::dimm_training_failure_type
*/
std::unordered_map<uint8_t, std::pair<std::string, EventToMsgMap_t>>
    dimmTrainingFailureTypeMap = {
        {training_failure::PHY_TRAINING_FAILURE_TYPE,
         std::make_pair("PHY training failure",
                        phyTrainingFailureSyndromeToMsgMap)},
        {training_failure::DIMM_TRAINING_FAILURE_TYPE,
         std::make_pair("DIMM training failure",
                        dimmTrainingFailureSyndromeToMsgMap)}};

/*
    A map between log level and the registry used for Redfish SEL log
    Using pldm::oem::log_level
*/
std::unordered_map<log_level, std::string> logLevelToRedfishMsgIdMap = {
    {log_level::OK, ampereEventRegistry},
    {log_level::WARNING, ampereWarningRegistry},
    {log_level::CRITICAL, ampereCriticalRegistry},
    {log_level::BIOSFWPANIC, BIOSFWPanicRegistry}};

std::unordered_map<
    uint16_t,
    std::vector<std::pair<
        std::string,
        std::unordered_map<uint8_t, std::pair<log_level, std::string>>>>>
    stateSensorToMsgMap = {
        {SOC_HEALTH_AVAILABILITY,
         {{"SoC Health",
           {{1, {log_level::OK, "Normal"}},
            {2, {log_level::WARNING, "Non-Critical"}},
            {3, {log_level::CRITICAL, "Critical"}},
            {4, {log_level::CRITICAL, "Fatal"}}}},
          {"SoC Availability",
           {{1, {log_level::OK, "Enabled"}},
            {2, {log_level::WARNING, "Disabled"}},
            {3, {log_level::CRITICAL, "Shutdown"}}}}}}};

std::string
    OemEventManager::prefixMsgStrCreation(pldm_tid_t tid, uint16_t sensorId)
{
    std::string description;
    if (!tidToSocketNameMap.contains(tid))
    {
        description += "TID " + std::to_string(tid) + ": ";
    }
    else
    {
        description += tidToSocketNameMap[tid] + ": ";
    }

    if (!sensorIdToStrMap.contains(sensorId))
    {
        description += "Sensor ID " + std::to_string(sensorId) + ": ";
    }
    else
    {
        description += sensorIdToStrMap[sensorId] + ": ";
    }

    return description;
}

void OemEventManager::sendJournalRedfish(const std::string& description,
                                         log_level& logLevel)
{
    if (description.empty())
    {
        return;
    }

    if (!logLevelToRedfishMsgIdMap.contains(logLevel))
    {
        lg2::error("Invalid {LEVEL} Description {DES}", "LEVEL", logLevel,
                   "DES", description);
        return;
    }
    auto redfishMsgId = logLevelToRedfishMsgIdMap[logLevel];
    lg2::info("MESSAGE={DES}", "DES", description, "REDFISH_MESSAGE_ID",
              redfishMsgId, "REDFISH_MESSAGE_ARGS", description);
}

std::string OemEventManager::dimmIdxsToString(uint32_t dimmIdxs)
{
    std::string description;
    for (const auto bitIdx : std::views::iota(0, maxDIMMIdxBitNum))
    {
        if (dimmIdxs & (static_cast<uint32_t>(1) << bitIdx))
        {
            description += " #" + std::to_string(bitIdx);
        }
    }
    return description;
}

void OemEventManager::handleBootOverallEvent(
    pldm_tid_t /*tid*/, uint16_t /*sensorId*/, uint32_t presentReading)
{
    log_level logLevel{log_level::OK};
    std::string description;
    std::stringstream strStream;

    uint8_t byte0 = (presentReading & 0x000000ff);
    uint8_t byte1 = (presentReading & 0x0000ff00) >> 8;
    uint8_t byte2 = (presentReading & 0x00ff0000) >> 16;
    uint8_t byte3 = (presentReading & 0xff000000) >> 24;
    /*
     * Handle SECpro, Mpro, ATF BL1, ATF BL2, ATF BL31,
     * ATF BL32 and DDR initialization
     */
    if (bootStageToMsgMap.contains(byte3))
    {
        // Boot stage adding
        description += bootStageToMsgMap[byte3];

        switch (byte3)
        {
            case boot_stage::DDR_TRAINING:
                if (byte0 >= ddrTrainingMsg.size())
                {
                    logLevel = log_level::BIOSFWPANIC;
                    description += " unknown status";
                }
                else
                {
                    description += ddrTrainingMsg[byte0];
                }
                if (0x01 == byte0)
                {
                    // Add complete percentage
                    description += " at " + std::to_string(byte1) + "%";
                }
                break;
            case boot_stage::S0_DDR_TRAINING_FAILURE:
            case boot_stage::S1_DDR_TRAINING_FAILURE:
                // ddr_training_status_msg()
                logLevel = log_level::BIOSFWPANIC;
                description += " at DIMMs:";
                // dimmIdxs = presentReading & 0x00ffffff;
                description += dimmIdxsToString(presentReading & 0x00ffffff);
                description += " of socket ";
                description +=
                    (boot_stage::S0_DDR_TRAINING_FAILURE == byte3) ? "0" : "1";
                break;
            default:
                if (byte0 >= bootStatMsg.size())
                {
                    logLevel = log_level::BIOSFWPANIC;
                    description += " unknown status";
                }
                else
                {
                    description += bootStatMsg[byte0];
                }
                break;
        }

        // Sensor report action is fail
        if (boot::status::BOOT_STATUS_FAILURE == byte2)
        {
            logLevel = log_level::BIOSFWPANIC;
        }
    }
    else
    {
        if (byte3 <= boot_stage::UEFI_STATUS_CLASS_CODE_MAX)
        {
            description +=
                bootStageToMsgMap[boot_stage::UEFI_STATUS_CLASS_CODE_MIN];

            strStream
                << "Segment (0x" << std::setfill('0') << std::hex
                << std::setw(8) << static_cast<uint32_t>(presentReading)
                << "); Status Class (0x" << std::setw(2)
                << static_cast<uint32_t>(byte3) << "); Status SubClass (0x"
                << std::setw(2) << static_cast<uint32_t>(byte2)
                << "); Operation Code (0x" << std::setw(4)
                << static_cast<uint32_t>((presentReading & 0xffff0000) >> 16)
                << ")" << std::dec;

            description += strStream.str();
        }
    }

    // Log to Redfish event
    sendJournalRedfish(description, logLevel);
}

int OemEventManager::processNumericSensorEvent(
    pldm_tid_t tid, uint16_t sensorId, const uint8_t* sensorData,
    size_t sensorDataLength)
{
    uint8_t eventState = 0;
    uint8_t previousEventState = 0;
    uint8_t sensorDataSize = 0;
    uint32_t presentReading;
    auto rc = decode_numeric_sensor_data(
        sensorData, sensorDataLength, &eventState, &previousEventState,
        &sensorDataSize, &presentReading);
    if (rc)
    {
        lg2::error(
            "Failed to decode numericSensorState event for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        return rc;
    }

    // DIMMx_Status sensorID 4+2*index (index 0 -> maxDIMMInstantNum-1)
    if (auto dimmIdx = (sensorId - 4) / 2;
        sensorId >= 4 && dimmIdx >= 0 && dimmIdx < maxDIMMInstantNum)
    {
        handleDIMMStatusEvent(tid, sensorId, presentReading);
        return PLDM_SUCCESS;
    }

    switch (sensorId)
    {
        case BOOT_OVERALL:
            handleBootOverallEvent(tid, sensorId, presentReading);
            break;
        case PCIE_HOT_PLUG:
            handlePCIeHotPlugEvent(tid, sensorId, presentReading);
            break;
        case DDR_STATUS:
            handleDDRStatusEvent(tid, sensorId, presentReading);
            break;
        case PCP_VR_STATE:
        case SOC_VR_STATE:
        case DPHY_VR1_STATE:
        case DPHY_VR2_STATE:
        case D2D_VR_STATE:
        case IOC_VR1_STATE:
        case IOC_VR2_STATE:
        case PCI_D_VR_STATE:
        case PCI_A_VR_STATE:
            handleVRDStatusEvent(tid, sensorId, presentReading);
            break;
        default:
            std::string description;
            std::stringstream strStream;
            log_level logLevel = log_level::OK;

            description += "SENSOR_EVENT : NUMERIC_SENSOR_STATE: ";
            description += prefixMsgStrCreation(tid, sensorId);
            strStream << std::setfill('0') << std::hex << "eventState 0x"
                      << std::setw(2) << static_cast<uint32_t>(eventState)
                      << " previousEventState 0x" << std::setw(2)
                      << static_cast<uint32_t>(previousEventState)
                      << " sensorDataSize 0x" << std::setw(2)
                      << static_cast<uint32_t>(sensorDataSize)
                      << " presentReading 0x" << std::setw(8)
                      << static_cast<uint32_t>(presentReading) << std::dec;
            description += strStream.str();

            sendJournalRedfish(description, logLevel);
            break;
    }
    return PLDM_SUCCESS;
}

int OemEventManager::processStateSensorEvent(pldm_tid_t tid, uint16_t sensorId,
                                             const uint8_t* sensorData,
                                             size_t sensorDataLength)
{
    uint8_t sensorOffset = 0;
    uint8_t eventState = 0;
    uint8_t previousEventState = 0;

    auto rc =
        decode_state_sensor_data(sensorData, sensorDataLength, &sensorOffset,
                                 &eventState, &previousEventState);
    if (rc)
    {
        lg2::error(
            "Failed to decode stateSensorState event for terminus ID {TID}, error {RC}",
            "TID", tid, "RC", rc);
        return rc;
    }

    std::string description;
    log_level logLevel = log_level::OK;

    if (stateSensorToMsgMap.contains(sensorId))
    {
        description += prefixMsgStrCreation(tid, sensorId);
        auto componentMap = stateSensorToMsgMap[sensorId];
        if (sensorOffset < componentMap.size())
        {
            description += std::get<0>(componentMap[sensorOffset]);
            auto stateMap = std::get<1>(componentMap[sensorOffset]);
            if (stateMap.contains(eventState))
            {
                logLevel = std::get<0>(stateMap[eventState]);
                description += " state : " + std::get<1>(stateMap[eventState]);
                if (stateMap.contains(previousEventState))
                {
                    description += "; previous state: " +
                                   std::get<1>(stateMap[previousEventState]);
                }
            }
            else
            {
                description += " sends unsupported event state: " +
                               std::to_string(eventState);
                if (stateMap.contains(previousEventState))
                {
                    description += "; previous state: " +
                                   std::get<1>(stateMap[previousEventState]);
                }
            }
        }
        else
        {
            description += "sends unsupported component sensor offset " +
                           std::to_string(sensorOffset);
        }
    }
    else
    {
        std::stringstream strStream;
        description += "SENSOR_EVENT : STATE_SENSOR_STATE: ";
        description += prefixMsgStrCreation(tid, sensorId);
        strStream << std::setfill('0') << std::hex << "sensorOffset 0x"
                  << std::setw(2) << static_cast<uint32_t>(sensorOffset)
                  << "eventState 0x" << std::setw(2)
                  << static_cast<uint32_t>(eventState)
                  << " previousEventState 0x" << std::setw(2)
                  << static_cast<uint32_t>(previousEventState) << std::dec;
        description += strStream.str();
    }

    sendJournalRedfish(description, logLevel);

    return PLDM_SUCCESS;
}

int OemEventManager::processSensorOpStateEvent(
    pldm_tid_t tid, uint16_t sensorId, const uint8_t* sensorData,
    size_t sensorDataLength)
{
    uint8_t present_op_state = 0;
    uint8_t previous_op_state = 0;

    auto rc = decode_sensor_op_data(sensorData, sensorDataLength,
                                    &present_op_state, &previous_op_state);
    if (rc)
    {
        lg2::error(
            "Failed to decode sensorOpState event for terminus ID {TID}, error {RC}",
            "TID", tid, "RC", rc);
        return rc;
    }

    std::string description;
    std::stringstream strStream;
    log_level logLevel = log_level::OK;

    description += "SENSOR_EVENT : SENSOR_OP_STATE: ";
    description += prefixMsgStrCreation(tid, sensorId);
    strStream << std::setfill('0') << std::hex << "present_op_state 0x"
              << std::setw(2) << static_cast<uint32_t>(present_op_state)
              << "previous_op_state 0x" << std::setw(2)
              << static_cast<uint32_t>(previous_op_state) << std::dec;
    description += strStream.str();

    sendJournalRedfish(description, logLevel);

    return PLDM_SUCCESS;
}

int OemEventManager::handleSensorEvent(
    const pldm_msg* request, size_t payloadLength, uint8_t /* formatVersion */,
    pldm_tid_t tid, size_t eventDataOffset)
{
    /* This OEM event handler is only used for SoC terminus*/
    if (!tidToSocketNameMap.contains(tid))
    {
        return PLDM_SUCCESS;
    }
    auto eventData =
        reinterpret_cast<const uint8_t*>(request->payload) + eventDataOffset;
    auto eventDataSize = payloadLength - eventDataOffset;

    uint16_t sensorId = 0;
    uint8_t sensorEventClassType = 0;
    size_t eventClassDataOffset = 0;
    auto rc =
        decode_sensor_event_data(eventData, eventDataSize, &sensorId,
                                 &sensorEventClassType, &eventClassDataOffset);
    if (rc)
    {
        lg2::error("Failed to decode sensor event data return code {RC}.", "RC",
                   rc);
        return rc;
    }
    const uint8_t* sensorData = eventData + eventClassDataOffset;
    size_t sensorDataLength = eventDataSize - eventClassDataOffset;

    switch (sensorEventClassType)
    {
        case PLDM_NUMERIC_SENSOR_STATE:
        {
            return processNumericSensorEvent(tid, sensorId, sensorData,
                                             sensorDataLength);
        }
        case PLDM_STATE_SENSOR_STATE:
        {
            return processStateSensorEvent(tid, sensorId, sensorData,
                                           sensorDataLength);
        }
        case PLDM_SENSOR_OP_STATE:
        {
            return processSensorOpStateEvent(tid, sensorId, sensorData,
                                             sensorDataLength);
        }
        default:
            std::string description;
            std::stringstream strStream;
            log_level logLevel = log_level::OK;

            description += "SENSOR_EVENT : Unsupported Sensor Class " +
                           std::to_string(sensorEventClassType) + ": ";
            description += prefixMsgStrCreation(tid, sensorId);
            strStream << std::setfill('0') << std::hex
                      << std::setw(sizeof(sensorData) * 2) << "Sensor data: ";

            auto dataPtr = sensorData;
            for ([[maybe_unused]] const auto& i :
                 std::views::iota(0, (int)sensorDataLength))
            {
                strStream << "0x" << static_cast<uint32_t>(*dataPtr);
                dataPtr += sizeof(sensorData);
            }

            description += strStream.str();

            sendJournalRedfish(description, logLevel);
    }
    lg2::info("Unsupported class type {CLASSTYPE}", "CLASSTYPE",
              sensorEventClassType);
    return PLDM_ERROR;
}

void OemEventManager::handlePCIeHotPlugEvent(pldm_tid_t tid, uint16_t sensorId,
                                             uint32_t presentReading)
{
    std::string description;
    std::stringstream strStream;
    PCIeHotPlugEventRecord_t record{presentReading};

    std::string sAction = (!record.bits.action) ? "Insertion" : "Removal";
    std::string sOpStatus = (!record.bits.opStatus) ? "Successful" : "Failed";
    log_level logLevel =
        (!record.bits.opStatus) ? log_level::OK : log_level::WARNING;

    description += prefixMsgStrCreation(tid, sensorId);

    strStream << "Segment (0x" << std::setfill('0') << std::hex << std::setw(2)
              << static_cast<uint32_t>(record.bits.segment) << "); Bus (0x"
              << std::setw(2) << static_cast<uint32_t>(record.bits.bus)
              << "); Device (0x" << std::setw(2)
              << static_cast<uint32_t>(record.bits.device) << "); Function (0x"
              << std::setw(2) << static_cast<uint32_t>(record.bits.function)
              << "); Action (" << sAction << "); Operation status ("
              << sOpStatus << "); Media slot number (" << std::dec
              << static_cast<uint32_t>(record.bits.mediaSlot) << ")";

    description += strStream.str();

    // Log to Redfish event
    sendJournalRedfish(description, logLevel);
}

std::string OemEventManager::dimmTrainingFailureToMsg(uint32_t failureInfo)
{
    std::string description;
    DIMMTrainingFailure_t failure{failureInfo};

    if (dimmTrainingFailureTypeMap.contains(failure.bits.type))
    {
        auto failureInfoMap = dimmTrainingFailureTypeMap[failure.bits.type];

        description += std::get<0>(failureInfoMap);

        description += "; MCU rank index " +
                       std::to_string(failure.bits.mcuRankIdx);

        description += "; Slice number " +
                       std::to_string(failure.bits.sliceNum);

        description += "; Upper nibble error status: ";
        description += (!failure.bits.upperNibbStatErr)
                           ? "No error"
                           : "Found no rising edge";

        description += "; Lower nibble error status: ";
        description += (!failure.bits.lowerNibbStatErr)
                           ? "No error"
                           : "Found no rising edge";

        description += "; Failure syndrome 0: ";

        auto& syndromeMap = std::get<1>(failureInfoMap);
        if (syndromeMap.contains(failure.bits.syndrome))
        {
            description += syndromeMap[failure.bits.syndrome];
        }
        else
        {
            description += "(Unknown syndrome)";
        }
    }
    else
    {
        description += "Unknown training failure type " +
                       std::to_string(failure.bits.type);
    }

    return description;
}

void OemEventManager::handleDIMMStatusEvent(pldm_tid_t tid, uint16_t sensorId,
                                            uint32_t presentReading)
{
    log_level logLevel{log_level::WARNING};
    std::string description;
    uint8_t byte3 = (presentReading & 0xff000000) >> 24;
    uint32_t byte012 = presentReading & 0xffffff;

    description += prefixMsgStrCreation(tid, sensorId);

    uint8_t dimmIdx = (sensorId - 4) / 2;

    description += "DIMM " + std::to_string(dimmIdx) + " ";

    if (dimmStatusToMsgMap.contains(byte3))
    {
        if (byte3 == dimm_status::INSTALLED_NO_ERROR ||
            byte3 == dimm_status::INSTALLED_BUT_DISABLED)
        {
            logLevel = log_level::OK;
        }

        description += dimmStatusToMsgMap[byte3];

        if (byte3 == dimm_status::TRAINING_FAILURE)
        {
            description += "; " + dimmTrainingFailureToMsg(byte012);
        }
        else if (byte3 == dimm_status::PMIC_TEMP_ALERT)
        {
            uint8_t byte0 = (byte012 & 0xff);
            if (byte0 < pmicTempAlertMsg.size())
            {
                description += ": " + pmicTempAlertMsg[byte0];
            }
        }
    }
    else
    {
        switch (byte3)
        {
            case dimm_status::PMIC_HIGH_TEMP:
                if (byte012 == 0x01)
                {
                    description += "has PMIC high temp condition";
                }
                break;
            case dimm_status::TSx_HIGH_TEMP:
                switch (byte012)
                {
                    case 0x01:
                        description += "has TS0";
                        break;
                    case 0x02:
                        description += "has TS1";
                        break;
                    case 0x03:
                        description += "has TS0 and TS1";
                        break;
                }
                description += " exceeding their high temperature threshold";
                break;
            case dimm_status::SPD_HUB_HIGH_TEMP:
                if (byte012 == 0x01)
                {
                    description += "has SPD/HUB high temp condition";
                }
                break;
            default:
                description += "has unsupported status " +
                               std::to_string(byte3);
                break;
        }
    }

    // Log to Redfish event
    sendJournalRedfish(description, logLevel);
}

void OemEventManager::handleDDRStatusEvent(pldm_tid_t tid, uint16_t sensorId,
                                           uint32_t presentReading)
{
    log_level logLevel{log_level::WARNING};
    std::string description;
    uint8_t byte3 = (presentReading & 0xff000000) >> 24;
    uint32_t byte012 = presentReading & 0xffffff;

    description += prefixMsgStrCreation(tid, sensorId);

    description += "DDR ";
    if (ddrStatusToMsgMap.contains(byte3))
    {
        if (byte3 == ddr_status::NO_SYSTEM_LEVEL_ERROR)
        {
            logLevel = log_level::OK;
        }

        description += ddrStatusToMsgMap[byte3];

        if (byte3 == ddr_status::CONFIGURATION_FAILURE ||
            byte3 == ddr_status::TRAINING_FAILURE)
        {
            // List out failed DIMMs
            description += dimmIdxsToString(byte012);
        }
    }
    else
    {
        description += "has unsupported status " + std::to_string(byte3);
    }

    // Log to Redfish event
    sendJournalRedfish(description, logLevel);
}

void OemEventManager::handleVRDStatusEvent(pldm_tid_t tid, uint16_t sensorId,
                                           uint32_t presentReading)
{
    log_level logLevel{log_level::WARNING};
    std::string description;
    std::stringstream strStream;

    description += prefixMsgStrCreation(tid, sensorId);

    VRDStatus_t status{presentReading};

    if (status.bits.warning && status.bits.critical)
    {
        description += "A VR warning and a VR critical";
        logLevel = log_level::CRITICAL;
    }
    else
    {
        if (status.bits.warning)
        {
            description += "A VR warning";
        }
        else if (status.bits.critical)
        {
            description += "A VR critical";
            logLevel = log_level::CRITICAL;
        }
        else
        {
            description += "No VR warning or critical";
            logLevel = log_level::OK;
        }
    }
    description += " condition observed";

    strStream << "; VR status byte high is 0x" << std::setfill('0') << std::hex
              << std::setw(2)
              << static_cast<uint32_t>(status.bits.vr_status_byte_high)
              << "; VR status byte low is 0x" << std::setw(2)
              << static_cast<uint32_t>(status.bits.vr_status_byte_low)
              << "; Reading is 0x" << std::setw(2)
              << static_cast<uint32_t>(presentReading) << ";";

    description += strStream.str();

    // Log to Redfish event
    sendJournalRedfish(description, logLevel);
}

} // namespace oem_ampere
} // namespace pldm
