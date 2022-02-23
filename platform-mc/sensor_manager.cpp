#include "sensor_manager.hpp"

#include "terminus_manager.hpp"

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

requester::Coroutine SensorManager::doSensorPollingTask()
{
    uint64_t t0 = 0;
    uint64_t t1 = 0;
    uint64_t elapsed = pollingTime * 1000000;
    sd_event_now(event.get(), CLOCK_MONOTONIC, &t0);
    for (auto& terminus : termini)
    {
        for (auto sensor : terminus.second->numericSensors)
        {
            sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
            sensor->elapsedTime += elapsed + (t1 - t0);
            if (sensor->elapsedTime >= sensor->updateTime)
            {
                co_await getSensorReading(sensor);
                sensor->elapsedTime = 0;
            }
        }
    }
}

requester::Coroutine
    SensorManager::getSensorReading(std::shared_ptr<NumericSensor> sensor)
{
    auto eid = sensor->eid;
    auto sensorId = sensor->sensorId;
    auto instanceId = requester.getInstanceId(eid);
    Request request(sizeof(pldm_msg_hdr) + PLDM_GET_SENSOR_READING_REQ_BYTES);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
    auto rc =
        encode_get_sensor_reading_req(instanceId, sensorId, 0x0, requestMsg);
    if (rc)
    {
        requester.markFree(eid, instanceId);
        std::cerr << "encode_get_sensor_reading_req failed, EID="
                  << unsigned(eid) << ", RC=" << rc << std::endl;
        co_return rc;
    }

    const pldm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await requester::SendRecvPldmMsg<RequesterHandler>(
        handler, eid, request, &responseMsg, &responseLen);
    if (rc)
    {
        std::cerr << "sendRecvPldmMsg failed. rc=" << static_cast<unsigned>(rc)
                  << "\n";
        co_return rc;
    }

    uint8_t completionCode = PLDM_SUCCESS;
    uint8_t sensorDataSize = 0;
    uint8_t sensorOperationalState = 0;
    uint8_t sensorEventMessageEnable = 0;
    uint8_t presentState = 0;
    uint8_t previousState = 0;
    uint8_t eventState = 0;
    union_sensor_data_size presentReading;
    rc = decode_get_sensor_reading_resp(
        responseMsg, responseLen, &completionCode, &sensorDataSize,
        &sensorOperationalState, &sensorEventMessageEnable, &presentState,
        &previousState, &eventState,
        reinterpret_cast<uint8_t*>(&presentReading));
    if (rc)
    {
        std::cerr << "Failed to decode response of GetSensorReading, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
        sensor->handleErrGetSensorReading();
        co_return rc;
    }

    switch (sensorOperationalState)
    {
        case PLDM_SENSOR_ENABLED:
            break;
        case PLDM_SENSOR_DISABLED:
            sensor->updateReading(true, false);
            co_return completionCode;
        case PLDM_SENSOR_UNAVAILABLE:
        default:
            sensor->updateReading(false, false);
            co_return completionCode;
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
    co_return completionCode;
}
} // namespace platform_mc
} // namespace pldm
