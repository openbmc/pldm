#include "oem_event_manager.hpp"

#include "common/utils.hpp"
#include "platform-mc/manager.hpp"

#include <libpldm/platform.h>

#include <phosphor-logging/lg2.hpp>

#include <exception>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace oem_arm
{

namespace
{

using BootRawValue = std::tuple<std::vector<uint8_t>, std::vector<uint8_t>>;

enum class StateSensorEventType
{
    PLDM_FILE_STATE_SENSOR_CRASHLOG,
    PLDM_FILE_STATE_SENSOR_UNSUPPORTED,
};

constexpr auto bootProgressTerminusName = "PHX";
constexpr uint16_t bootProgressSensorId = 1;
constexpr uint16_t firstCrashlogStateSensorId = 2;
constexpr uint8_t crashlogStateSensorStride = 2;
constexpr uint8_t deviceFileNotChangedState = 1;
constexpr uint8_t deviceFileUpdatedState = 2;
constexpr auto bootRawObjectPath = "/xyz/openbmc_project/state/boot/raw0";
constexpr auto bootRawInterface = "xyz.openbmc_project.State.Boot.Raw";
constexpr auto bootRawProperty = "Value";
constexpr auto bootProgressObjectPath = "/xyz/openbmc_project/state/host0";
constexpr auto bootProgressInterface =
    "xyz.openbmc_project.State.Boot.Progress";
constexpr auto bootProgressLastUpdateProperty = "BootProgressLastUpdate";
constexpr auto bootProgressProperty = "BootProgress";
constexpr auto bootProgressStageOem =
    "xyz.openbmc_project.State.Boot.Progress.ProgressStages.OEM";

bool isCrashlogFileStateSensor(uint16_t sensorId)
{
    if (sensorId < firstCrashlogStateSensorId)
    {
        return false;
    }

    auto offset = sensorId - firstCrashlogStateSensorId;
    return (offset % crashlogStateSensorStride) == 0;
}

StateSensorEventType getStateSensorEventType(uint16_t sensorId)
{
    if (isCrashlogFileStateSensor(sensorId))
    {
        return StateSensorEventType::PLDM_FILE_STATE_SENSOR_CRASHLOG;
    }

    return StateSensorEventType::PLDM_FILE_STATE_SENSOR_UNSUPPORTED;
}

std::vector<uint8_t> bootProgressToBytes(uint32_t presentReading)
{
    return {
        static_cast<uint8_t>((presentReading >> 24) & 0xff),
        static_cast<uint8_t>((presentReading >> 16) & 0xff),
        static_cast<uint8_t>((presentReading >> 8) & 0xff),
        static_cast<uint8_t>(presentReading & 0xff),
    };
}

} // namespace

int OemEventManager::handleSensorEvent(
    const pldm_msg* request, size_t payloadLength, uint8_t /* formatVersion */,
    pldm_tid_t tid, size_t eventDataOffset)
{
    lg2::info(
        "Received an Arm OEM sensor event from terminus {TID}. The payload "
        "length is {LEN} bytes and the event data starts at offset {OFFSET}",
        "TID", tid, "LEN", payloadLength, "OFFSET", eventDataOffset);

    // Validate the request before using request->payload, and make sure the
    // event data offset is inside the payload before computing eventData.
    if (request == nullptr || eventDataOffset > payloadLength)
    {
        lg2::error("Invalid Arm sensor event payload. The request is null or "
                   "the event data offset {OFFSET} exceeds the payload length "
                   "{LEN}",
                   "OFFSET", eventDataOffset, "LEN", payloadLength);
        return PLDM_ERROR_INVALID_LENGTH;
    }

    if (!isPhxTerminus(tid))
    {
        lg2::info("Ignoring Arm OEM sensor event from terminus {TID}; "
                  "Arm OEM handles only sensor events from the PHX terminus",
                  "TID", tid);
        return PLDM_SUCCESS;
    }

    const auto* eventData =
        reinterpret_cast<const uint8_t*>(request->payload) + eventDataOffset;
    auto eventDataSize = payloadLength - eventDataOffset;

    return decodeSensorEvent(tid, eventData, eventDataSize);
}

bool OemEventManager::isPhxTerminus(pldm_tid_t tid) const
{
    if (!manager)
    {
        lg2::debug("Unable to resolve Arm terminus {TID}; manager is null",
                   "TID", tid);
        return false;
    }

    auto terminusName = manager->getTerminusName(tid);
    if (!terminusName.has_value())
    {
        lg2::debug("Unable to resolve Arm terminus name for terminus {TID}",
                   "TID", tid);
        return false;
    }

    lg2::debug("Resolved Arm terminus {TID} to name {NAME}", "TID", tid, "NAME",
               terminusName.value());

    return terminusName.value() == bootProgressTerminusName;
}

int OemEventManager::decodeSensorEvent(pldm_tid_t tid, const uint8_t* eventData,
                                       size_t eventDataSize)
{
    if (eventData == nullptr)
    {
        lg2::error("Invalid Arm sensor event data from terminus {TID}", "TID",
                   tid);
        return PLDM_ERROR_INVALID_DATA;
    }

    uint16_t sensorId = 0;
    uint8_t sensorEventClassType = 0;
    size_t eventClassDataOffset = 0;
    auto rc =
        decode_sensor_event_data(eventData, eventDataSize, &sensorId,
                                 &sensorEventClassType, &eventClassDataOffset);
    if (rc)
    {
        lg2::error("Failed to decode Arm sensor event data, error {RC}", "RC",
                   rc);
        return rc;
    }

    lg2::debug("Decoded Arm sensor event from terminus {TID}. Sensor {SID} "
               "reported class {CLASS} with class data offset {OFFSET}",
               "TID", tid, "SID", sensorId, "CLASS", sensorEventClassType,
               "OFFSET", eventClassDataOffset);

    if (eventClassDataOffset > eventDataSize)
    {
        lg2::error("Invalid Arm sensor event data. Class data offset {OFFSET} "
                   "exceeds event data size {SIZE}",
                   "OFFSET", eventClassDataOffset, "SIZE", eventDataSize);
        return PLDM_ERROR_INVALID_LENGTH;
    }

    const auto* sensorData = eventData + eventClassDataOffset;
    auto sensorDataLength = eventDataSize - eventClassDataOffset;

    if (sensorEventClassType == PLDM_NUMERIC_SENSOR_STATE)
    {
        return processNumericSensorEvent(tid, sensorId, sensorData,
                                         sensorDataLength);
    }

    if (sensorEventClassType == PLDM_STATE_SENSOR_STATE)
    {
        return processStateSensorEvent(tid, sensorId, sensorData,
                                       sensorDataLength);
    }

    return PLDM_SUCCESS;
}

int OemEventManager::processNumericSensorEvent(
    pldm_tid_t tid, uint16_t sensorId, const uint8_t* sensorData,
    size_t sensorDataLength)
{
    if (sensorData == nullptr)
    {
        lg2::error(
            "Invalid Arm numeric sensor event data from terminus {TID}, sensor "
            "{SID}",
            "TID", tid, "SID", sensorId);
        return PLDM_ERROR_INVALID_DATA;
    }

    uint8_t eventState = 0;
    uint8_t previousEventState = 0;
    uint8_t sensorDataSize = 0;
    uint32_t presentReading = 0;
    auto rc = decode_numeric_sensor_data(
        sensorData, sensorDataLength, &eventState, &previousEventState,
        &sensorDataSize, &presentReading);
    if (rc)
    {
        lg2::error(
            "Failed to decode Arm numeric sensor event for terminus {TID}, "
            "sensor {SID}, error {RC}",
            "TID", tid, "SID", sensorId, "RC", rc);
        return rc;
    }

    switch (sensorId)
    {
        case bootProgressSensorId:
            lg2::info("Arm boot progress code event from terminus {TID}. "
                      "Sensor {SID} reported present reading {READING}",
                      "TID", tid, "SID", sensorId, "READING", presentReading);
            return updateBootProgress(presentReading);
        default:
            lg2::debug(
                "Ignoring unsupported Arm numeric sensor event from terminus "
                "{TID}, sensor {SID}",
                "TID", tid, "SID", sensorId);
            return PLDM_SUCCESS;
    }
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
            "Failed to decode Arm state sensor event for terminus {TID}, "
            "sensor {SID}, error {RC}",
            "TID", tid, "SID", sensorId, "RC", rc);
        return rc;
    }

    switch (getStateSensorEventType(sensorId))
    {
        case StateSensorEventType::PLDM_FILE_STATE_SENSOR_CRASHLOG:
            if (eventState == deviceFileNotChangedState)
            {
                lg2::debug("Device File state reset to NotChanged from "
                           "terminus {TID}, sensor {SID}",
                           "TID", tid, "SID", sensorId);
                return PLDM_SUCCESS;
            }

            if (eventState == deviceFileUpdatedState)
            {
                lg2::info("Crashlog Device File state event from terminus "
                          "{TID}, sensor {SID}",
                          "TID", tid, "SID", sensorId);
                return PLDM_SUCCESS;
            }

            lg2::debug("Ignoring unsupported Device File state {STATE} from "
                       "terminus {TID}, sensor {SID}",
                       "STATE", eventState, "TID", tid, "SID", sensorId);
            return PLDM_SUCCESS;
        case StateSensorEventType::PLDM_FILE_STATE_SENSOR_UNSUPPORTED:
            lg2::debug(
                "Ignoring unsupported Arm state sensor event from terminus "
                "{TID}, sensor {SID}",
                "TID", tid, "SID", sensorId);
            return PLDM_SUCCESS;
    }

    return PLDM_SUCCESS;
}

int OemEventManager::updateBootProgress(uint32_t presentReading) const
{
    auto postCode = bootProgressToBytes(presentReading);

    try
    {
        auto& bus = pldm::utils::DBusHandler::getBus();
        pldm::utils::DBusHandler dbusHandler;
        auto service =
            dbusHandler.getService(bootRawObjectPath, bootRawInterface);
        auto method = bus.new_method_call(service.c_str(), bootRawObjectPath,
                                          pldm::utils::dbusProperties, "Set");
        method.append(bootRawInterface, bootRawProperty,
                      std::variant<BootRawValue>(
                          BootRawValue{postCode, std::vector<uint8_t>{}}));
        bus.call_noreply(method, dbusTimeout);

        pldm::utils::DBusMapping bootProgressLastUpdate = {
            bootProgressObjectPath, bootProgressInterface,
            bootProgressLastUpdateProperty, "uint64_t"};
        dbusHandler.setDbusProperty(bootProgressLastUpdate,
                                    pldm::utils::getCurrentSystemTimeUsec());

        pldm::utils::DBusMapping bootProgress = {
            bootProgressObjectPath, bootProgressInterface, bootProgressProperty,
            "string"};
        dbusHandler.setDbusProperty(bootProgress,
                                    std::string{bootProgressStageOem});
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Failed to update boot progress from Arm boot progress code event: "
            "{ERROR}",
            "ERROR", e);
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

} // namespace oem_arm
} // namespace pldm
