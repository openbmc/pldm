#include "oem_event_manager.hpp"

#include "common/utils.hpp"
#include "platform-mc/manager.hpp"

#include <libpldm/platform.h>

#include <phosphor-logging/lg2.hpp>

#include <chrono>
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

constexpr auto bootProgressTerminusName = "PHX";
constexpr uint16_t bootProgressSensorId = 1;
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

std::vector<uint8_t> bootProgressToBytes(uint32_t presentReading)
{
    return {
        static_cast<uint8_t>((presentReading >> 24) & 0xff),
        static_cast<uint8_t>((presentReading >> 16) & 0xff),
        static_cast<uint8_t>((presentReading >> 8) & 0xff),
        static_cast<uint8_t>(presentReading & 0xff),
    };
}

uint64_t getCurrentTimestamp()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

void setBootProgressProperty(auto& bus, const std::string& service,
                             const char* property,
                             const std::variant<uint64_t, std::string>& value)
{
    auto method = bus.new_method_call(service.c_str(), bootProgressObjectPath,
                                      pldm::utils::dbusProperties, "Set");
    method.append(bootProgressInterface, property, value);
    bus.call_noreply(method, dbusTimeout);
}

} // namespace

int OemEventManager::handleSensorEvent(
    const pldm_msg* request, size_t payloadLength, uint8_t /* formatVersion */,
    pldm_tid_t tid, size_t eventDataOffset)
{
    if (request == nullptr || eventDataOffset > payloadLength)
    {
        lg2::error(
            "Invalid Arm sensor event payload length {LEN}, offset {OFFSET}",
            "LEN", payloadLength, "OFFSET", eventDataOffset);
        return PLDM_ERROR_INVALID_LENGTH;
    }

    if (!isPhxTerminus(tid))
    {
        lg2::debug("Ignoring Arm sensor event from terminus {TID}; "
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
    if (manager == nullptr)
    {
        return false;
    }

    auto terminusName = manager->getTerminusName(tid);
    if (!terminusName.has_value())
    {
        return false;
    }

    return terminusName.value() == bootProgressTerminusName;
}

int OemEventManager::decodeSensorEvent(pldm_tid_t tid, const uint8_t* eventData,
                                       size_t eventDataSize)
{
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

    if (sensorEventClassType != PLDM_NUMERIC_SENSOR_STATE)
    {
        return PLDM_SUCCESS;
    }

    const auto* sensorData = eventData + eventClassDataOffset;
    auto sensorDataLength = eventDataSize - eventClassDataOffset;

    return processNumericSensorEvent(tid, sensorId, sensorData,
                                     sensorDataLength);
}

int OemEventManager::processNumericSensorEvent(
    pldm_tid_t tid, uint16_t sensorId, const uint8_t* sensorData,
    size_t sensorDataLength)
{
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
            lg2::info(
                "Arm BootProgressCode event from terminus {TID}, sensor {SID}, "
                "present reading {READING}",
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

        auto bootProgressService = dbusHandler.getService(
            bootProgressObjectPath, bootProgressInterface);
        setBootProgressProperty(
            bus, bootProgressService, bootProgressLastUpdateProperty,
            std::variant<uint64_t, std::string>{getCurrentTimestamp()});
        setBootProgressProperty(bus, bootProgressService, bootProgressProperty,
                                std::variant<uint64_t, std::string>{
                                    std::string{bootProgressStageOem}});
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Failed to update Boot.Progress from Arm BootProgressCode event: "
            "{ERROR}",
            "ERROR", e);
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

} // namespace oem_arm
} // namespace pldm
