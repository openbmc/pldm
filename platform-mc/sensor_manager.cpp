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

    forceStopPolling = false;
}

void SensorManager::stopPolling()
{
    forceStopPolling = true;

    if (sensorPollTimer)
    {
        sensorPollTimer->stop();
    }
}

requester::Coroutine SensorManager::doSensorPollingTask()
{
    uint64_t t0 = 0;
    uint64_t t1 = 0;
    uint64_t elapsed = 0;
    uint64_t pollingTimeInUsec = pollingTime * 1000000;

    sd_event_now(event.get(), CLOCK_MONOTONIC, &t0);
    for (auto& terminus : termini)
    {
        if (forceStopPolling)
        {
            break;
        }

        for (auto& sensor : terminus.second->sensorObjects)
        {
            if (forceStopPolling)
            {
                break;
            }

            if (sensor->updateTime == std::numeric_limits<uint64_t>::max())
            {
                continue;
            }

            sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
            elapsed = t1 - t0;
            sensor->elapsedTime += (pollingTimeInUsec + elapsed);
            if (sensor->elapsedTime >= sensor->updateTime)
            {
                std::vector<uint8_t> request = sensor->getReadingRequest();
                if (request.empty())
                {
                    co_return PLDM_ERROR;
                }

                const pldm_msg* responseMsg = NULL;
                size_t responseLen = 0;
                auto rc = co_await terminusManager.SendRecvPldmMsg(
                    sensor->tid, request, &responseMsg, &responseLen);
                if (rc)
                {
                    co_return rc;
                }
                rc = sensor->handleReadingResponse(responseMsg, responseLen);
                if (rc)
                {
                    co_return rc;
                }

                if (sensorPollTimer && !sensorPollTimer->isRunning())
                {
                    co_return PLDM_ERROR;
                }
                sensor->elapsedTime = 0;
            }
        }
    }
}

} // namespace platform_mc
} // namespace pldm
