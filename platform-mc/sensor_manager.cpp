#include "sensor_manager.hpp"

namespace pldm
{
namespace platform_mc
{

using namespace std::chrono;

void SensorManager::startPolling()
{
    if (!sensorPollTimer)
    {
        sensorPollTimer = std::make_unique<phosphor::Timer>(
            event.get(),
            std::bind_front(&SensorManager::doSensorPolling, this));
    }

    if (!sensorPollTimer->isRunning())
    {
        sensorPollTimer->start(
            duration_cast<std::chrono::seconds>(seconds(pollingTime)), true);
    }
}

void SensorManager::stopPolling()
{
    if (sensorPollTimer)
    {
        sensorPollTimer->stop();
    }
}

void SensorManager::doSensorPolling()
{
    for (auto& terminus : termini)
    {
        for (auto sensor : terminus.second->numericSensors)
        {
            sensor->elapsedTime += pollingTime;
            if (sensor->elapsedTime >= sensor->updateTime)
            {
                sendGetSensorReading(sensor);
                sensor->elapsedTime = 0;
            }
        }
    }
}

void SensorManager::sendGetSensorReading(std::shared_ptr<NumericSensor> sensor)
{
    auto eid = sensor->eid;
    auto sensorId = sensor->sensorId;
    auto instanceId = requester.getInstanceId(eid);

    Request requestMsg(sizeof(pldm_msg_hdr) +
                       PLDM_GET_SENSOR_READING_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = encode_get_sensor_reading_req(instanceId, sensorId, 0x0, request);
    if (rc)
    {
        requester.markFree(eid, instanceId);
        std::cerr << "encode_get_sensor_reading_req failed, EID="
                  << unsigned(eid) << ", RC=" << rc << std::endl;
        return;
    }

    rc = handler.registerRequest(
        eid, instanceId, PLDM_PLATFORM, PLDM_GET_SENSOR_READING,
        std::move(requestMsg),
        std::move(std::bind_front(&SensorManager::handleRespGetSensorReading,
                                  this, sensorId)));
    if (rc)
    {
        std::cerr << "Failed to send GetSensorReading request, EID="
                  << unsigned(eid) << " SensorID=" << unsigned(sensorId)
                  << ", RC=" << rc << "\n ";
        sensor->handleErrGetSensorReading();
    }
}

void SensorManager::handleRespGetSensorReading(uint16_t sensorId,
                                               mctp_eid_t eid,
                                               const pldm_msg* response,
                                               size_t respMsgLen)
{
    // find out the sensor instance
    std::shared_ptr<NumericSensor> sensor = nullptr;
    for (auto& terminus : termini)
    {
        for (auto numericSensor : terminus.second->numericSensors)
        {
            if (numericSensor->eid == eid &&
                numericSensor->sensorId == sensorId)
            {
                sensor = numericSensor;
                break;
            }
        }
    }

    if (sensor == nullptr)
    {
        return;
    }

    if (response == nullptr || !respMsgLen)
    {
        std::cerr << "No response received for GetSensorReading, EID="
                  << unsigned(eid) << "\n";
        sensor->handleErrGetSensorReading();
        return;
    }

    uint8_t completionCode = PLDM_SUCCESS;
    uint8_t sensorDataSize = 0;
    uint8_t sensorOperationalState = 0;
    uint8_t sensorEventMessageEnable = 0;
    uint8_t presentState = 0;
    uint8_t previousState = 0;
    uint8_t eventState = 0;
    union_sensor_data_size presentReading;
    auto rc = decode_get_sensor_reading_resp(
        response, respMsgLen, &completionCode, &sensorDataSize,
        &sensorOperationalState, &sensorEventMessageEnable, &presentState,
        &previousState, &eventState,
        reinterpret_cast<uint8_t*>(&presentReading));
    if (rc)
    {
        std::cerr << "Failed to decode response of GetSensorReading, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
        sensor->handleErrGetSensorReading();
        return;
    }

    switch (sensorOperationalState)
    {
        case PLDM_SENSOR_DISABLED:
            sensor->updateReading(true, false);
            return;
        case PLDM_SENSOR_UNAVAILABLE:
            sensor->updateReading(false, false);
            return;
        case PLDM_SENSOR_ENABLED:
            break;
        default:
            return;
    }

    double value;
    switch (sensorDataSize)
    {
        case PLDM_SENSOR_DATA_SIZE_UINT8:
            value = static_cast<float>(presentReading.value_u8);
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT8:
            value = static_cast<float>(presentReading.value_s8);
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT16:
            value = static_cast<float>(le16toh(presentReading.value_u16));
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT16:
            value = static_cast<float>(le16toh(presentReading.value_s16));
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT32:
            value = static_cast<float>(le32toh(presentReading.value_u32));
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT32:
            value = static_cast<float>(le32toh(presentReading.value_s32));
            break;
        default:
            value = 0;
            break;
    }

    sensor->updateReading(true, true, value);
}

} // namespace platform_mc
} // namespace pldm
