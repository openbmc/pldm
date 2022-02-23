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
        sensorPollTimer->start(duration_cast<std::chrono::microseconds>(
                                   milliseconds(pollingTime * 1000)),
                               true);
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
                sendGetNumericSensorReading(sensor);
                sensor->elapsedTime = 0;
            }
        }
    }
}

void SensorManager::sendGetNumericSensorReading(
    std::shared_ptr<NumericSensor> sensor)
{
    auto instanceId = requester.getInstanceId(sensor->eid);
    Request requestMsg(sizeof(pldm_msg_hdr) +
                       PLDM_GET_SENSOR_READING_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = encode_get_sensor_reading_req(instanceId, sensor->sensorId, 0x0,
                                            request);
    if (rc)
    {
        requester.markFree(sensor->eid, instanceId);
        std::cerr << "encode_get_sensor_reading_req failed, EID="
                  << unsigned(sensor->eid) << ", RC=" << rc << std::endl;
        return;
    }

    rc = handler.registerRequest(
        sensor->eid, instanceId, PLDM_PLATFORM, PLDM_GET_SENSOR_READING,
        std::move(requestMsg),
        std::move(std::bind_front(&NumericSensor::handleRespGetSensorReading,
                                  sensor)));
    if (rc)
    {
        std::cerr << "Failed to send GetSensorReading request, EID="
                  << unsigned(sensor->eid)
                  << "SensorID=" << unsigned(sensor->sensorId) << ", RC=" << rc
                  << "\n ";
        sensor->handleErrGetSensorReading();
    }
}

} // namespace platform_mc
} // namespace pldm
