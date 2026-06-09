#include "oem_event_manager.hpp"

#include "common/utils.hpp"
#include "platform-mc/manager.hpp"

#include <libpldm/platform.h>

#include <phosphor-logging/lg2.hpp>

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

constexpr auto bootProgressCodeToken = "BootProgressCode";
constexpr auto bootRawObjectPath = "/xyz/openbmc_project/state/boot/raw0";
constexpr auto bootRawInterface = "xyz.openbmc_project.State.Boot.Raw";
constexpr auto bootRawProperty = "Value";

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
    if (request == nullptr || eventDataOffset > payloadLength)
    {
        lg2::error(
            "Invalid Arm sensor event payload length {LEN}, offset {OFFSET}",
            "LEN", payloadLength, "OFFSET", eventDataOffset);
        return PLDM_ERROR_INVALID_LENGTH;
    }

    const auto* eventData =
        reinterpret_cast<const uint8_t*>(request->payload) + eventDataOffset;
    auto eventDataSize = payloadLength - eventDataOffset;

    return handlePolledSensorEvent(tid, PLDM_PLATFORM_EVENT_ID_NULL, eventData,
                                   eventDataSize);
}

int OemEventManager::handlePolledSensorEvent(pldm_tid_t tid,
                                             uint16_t /* eventId */,
                                             const uint8_t* eventData,
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

int OemEventManager::processNumericSensorEvent(pldm_tid_t tid,
                                               uint16_t sensorId,
                                               const uint8_t* sensorData,
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
            "Failed to decode Arm numeric sensor event for terminus {TID}, sensor {SID}, error {RC}",
            "TID", tid, "SID", sensorId, "RC", rc);
        return rc;
    }

    if (!isBootProgressCodeSensor(tid, sensorId))
    {
        lg2::debug(
            "Ignoring Arm numeric sensor event for non-BootProgress sensor on terminus {TID}, sensor {SID}",
            "TID", tid, "SID", sensorId);
        return PLDM_SUCCESS;
    }

    lg2::info(
        "Arm BootProgressCode event from terminus {TID}, sensor {SID}, present reading {READING}",
        "TID", tid, "SID", sensorId, "READING", presentReading);

    return updateBootRawFromBootProgress(presentReading);
}

bool OemEventManager::isBootProgressCodeSensor(pldm_tid_t tid,
                                               uint16_t sensorId) const
{
    if (manager == nullptr)
    {
        return false;
    }

    auto sensorName = manager->getNumericSensorName(tid, sensorId);
    if (!sensorName.has_value())
    {
        return false;
    }

    return sensorName->find(bootProgressCodeToken) != std::string::npos;
}

int OemEventManager::updateBootRawFromBootProgress(
    uint32_t presentReading) const
{
    auto postCode = bootProgressToBytes(presentReading);

    try
    {
        auto& bus = pldm::utils::DBusHandler::getBus();
        auto service = pldm::utils::DBusHandler().getService(bootRawObjectPath,
                                                             bootRawInterface);
        auto method = bus.new_method_call(
            service.c_str(), bootRawObjectPath, pldm::utils::dbusProperties,
            "Set");
        method.append(bootRawInterface, bootRawProperty,
                      std::variant<BootRawValue>(
                          BootRawValue{postCode, std::vector<uint8_t>{}}));
        bus.call_noreply(method, dbusTimeout);
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Failed to update Boot.Raw from Arm BootProgressCode event: {ERROR}",
            "ERROR", e);
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

} // namespace oem_arm
} // namespace pldm
