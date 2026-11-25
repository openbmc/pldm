#include "sensor_manager.hpp"

#include "manager.hpp"
#include "terminus_manager.hpp"

#include <phosphor-logging/lg2.hpp>

#include <exception>

namespace pldm
{
namespace platform_mc
{

SensorManager::SensorManager(sdeventplus::Event& event,
                             TerminusManager& terminusManager,
                             TerminiMapper& termini, Manager* manager) :
    event(event), terminusManager(terminusManager), termini(termini),
    pollingTime(SENSOR_POLLING_TIME), manager(manager)
{}

void SensorManager::startPolling(pldm_tid_t tid)
{
    if (!termini.contains(tid))
    {
        return;
    }

    /* tid already initializes roundRobinSensors list */
    if (sensorPollTimers.contains(tid))
    {
        lg2::info("Terminus ID {TID}: sensor poll timer already exists.", "TID",
                  tid);
        return;
    }

    roundRobinSensorItMap[tid] = 0;

    updateAvailableState(tid, true);

    sensorPollTimers[tid] = std::make_unique<sdbusplus::Timer>(
        event.get(), [this, tid] { this->doSensorPolling(tid); });

    startSensorPollTimer(tid);
}

void SensorManager::startSensorPollTimer(pldm_tid_t tid)
{
    auto it = sensorPollTimers.find(tid);
    if (it == sensorPollTimers.end() || !it->second)
    {
        return;
    }

    try
    {
        if (!it->second->isRunning())
        {
            it->second->start(duration_cast<std::chrono::milliseconds>(
                                  std::chrono::milliseconds(pollingTime)),
                              true);
        }
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Terminus ID {TID}: Failed to start sensor polling timer. Exception: {EXCEPTION}",
            "TID", tid, "EXCEPTION", e);
        return;
    }
}

void SensorManager::disableTerminusSensors(pldm_tid_t tid)
{
    auto terminusIt = termini.find(tid);
    if (terminusIt == termini.end())
    {
        return;
    }

    // numeric sensor
    auto terminus = terminusIt->second;
    if (!terminus)
    {
        return;
    }

    for (auto& sensor : terminus->numericSensors)
    {
        sensor->updateReading(true, false,
                              std::numeric_limits<double>::quiet_NaN());
    }
}

void SensorManager::stopPolling(pldm_tid_t tid)
{
    /* Stop polling timer */
    auto timerIt = sensorPollTimers.find(tid);
    if (timerIt != sensorPollTimers.end())
    {
        if (timerIt->second != nullptr)
        {
            timerIt->second->stop();
        }
        sensorPollTimers.erase(timerIt);
    }

    roundRobinSensorItMap.erase(tid);

    auto handleIt = doSensorPollingTaskHandles.find(tid);
    if (handleIt != doSensorPollingTaskHandles.end())
    {
        handleIt->second->scope.request_stop();
        doSensorPollingTaskHandles.erase(handleIt);
        // The coroutine holds its own shared_ptr<PollHandle> as a parameter
        // (in the coroutine frame), so the scope outlives this erase and is
        // destroyed cleanly after final_suspend completes its bookkeeping.
    }

    availableState.erase(tid);
}

void SensorManager::doSensorPolling(pldm_tid_t tid)
{
    auto [it, inserted] = doSensorPollingTaskHandles.try_emplace(tid);
    if (!inserted && !it->second->rcOpt.has_value())
    {
        return;
    }

    auto handle = std::make_shared<PollHandle>();
    it->second = handle;
    handle->scope.spawn(
        [](SensorManager& self, pldm_tid_t tid,
           std::shared_ptr<PollHandle> handle) -> exec::task<void> {
            auto res = co_await stdexec::stopped_as_optional(
                self.doSensorPollingTask(tid));
            int rc = PLDM_SUCCESS;
            if (res.has_value())
            {
                rc = *res;
            }
            else
            {
                lg2::info("Stopped polling for Terminus ID {TID}", "TID", tid);
                try
                {
                    if (self.isSensorPollTimerRunning(tid))
                    {
                        self.sensorPollTimers[tid]->stop();
                    }
                }
                catch (const std::exception& e)
                {
                    lg2::error(
                        "Terminus ID {TID}: Failed to stop polling timer. Exception: {EXCEPTION}",
                        "TID", tid, "EXCEPTION", e);
                }
            }
            handle->rcOpt = rc;
        }(*this, tid, handle),
        exec::default_task_context<void>(stdexec::inline_scheduler{}));
}

bool SensorManager::isSensorPollTimerRunning(pldm_tid_t tid) const
{
    auto it = sensorPollTimers.find(tid);
    return it != sensorPollTimers.end() && it->second &&
           it->second->isRunning();
}

exec::task<int> SensorManager::doSensorPollingTask(pldm_tid_t tid)
{
    uint64_t t0 = 0;
    uint64_t t1 = 0;
    uint64_t elapsed = 0;
    uint64_t pollingTimeInUsec = pollingTime * 1000;
    uint8_t rc = PLDM_SUCCESS;

    do
    {
        if (!isSensorPollTimerRunning(tid))
        {
            co_return PLDM_ERROR;
        }

        sd_event_now(event.get(), CLOCK_MONOTONIC, &t0);

        /**
         * Terminus is not available for PLDM request.
         * The terminus manager will trigger recovery process to recovery the
         * communication between the local terminus and the remote terminus.
         * The sensor polling should be stopped while recovering the
         * communication.
         */
        if (!getAvailableState(tid))
        {
            lg2::info(
                "Terminus ID {TID} is not available for PLDM request from {NOW}.",
                "TID", tid, "NOW", pldm::utils::getCurrentSystemTime());
            co_await stdexec::just_stopped();
        }

        auto terminusIt = termini.find(tid);
        if (terminusIt == termini.end())
        {
            co_return PLDM_SUCCESS;
        }

        auto terminus = terminusIt->second;
        if (!terminus)
        {
            lg2::info(
                "Terminus ID {TID} does not have a valid Terminus object {NOW}.",
                "TID", tid, "NOW", pldm::utils::getCurrentSystemTime());
            co_return PLDM_ERROR;
        }

        if (manager && terminus->pollEvent)
        {
            co_await manager->pollForPlatformEvent(
                tid, terminus->pollEventId, terminus->pollDataTransferHandle);
        }

        if (manager && (!terminus->pollEvent))
        {
            co_await manager->oemPollForPlatformEvent(tid);
        }

        rc = co_await enableStateEffecters(tid, terminus);
        if (rc != PLDM_SUCCESS)
        {
            co_return rc;
        }

        sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);

        auto& numericSensors = terminus->numericSensors;
        auto toBeUpdated = numericSensors.size();

        auto sensorItEntry = roundRobinSensorItMap.find(tid);
        if (sensorItEntry == roundRobinSensorItMap.end())
        {
            lg2::info(
                "Terminus ID {TID} does not have a round robin sensor iteration {NOW}.",
                "TID", tid, "NOW", pldm::utils::getCurrentSystemTime());
            co_return PLDM_ERROR;
        }
        auto& sensorIt = sensorItEntry->second;

        while (((t1 - t0) < pollingTimeInUsec) && (toBeUpdated > 0))
        {
            if (!getAvailableState(tid))
            {
                lg2::info(
                    "Terminus ID {TID} is not available for PLDM request from {NOW}.",
                    "TID", tid, "NOW", pldm::utils::getCurrentSystemTime());
                co_await stdexec::just_stopped();
            }

            if (sensorIt >= numericSensors.size())
            {
                sensorIt = 0;
            }

            auto sensor = numericSensors[sensorIt];

            sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
            elapsed = t1 - sensor->timeStamp;
            if ((sensor->updateTime <= elapsed) || (!sensor->timeStamp))
            {
                rc = co_await getSensorReading(sensor);

                if (!isSensorPollTimerRunning(tid))
                {
                    co_return PLDM_ERROR;
                }
                sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
                if (rc == PLDM_SUCCESS)
                {
                    sensor->timeStamp = t1;
                }
                else
                {
                    lg2::error(
                        "Failed to get sensor value for terminus {TID}, error: {RC}",
                        "TID", tid, "RC", rc);
                }
            }

            toBeUpdated--;
            sensorIt++;

            sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
        }

        sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
    } while ((t1 - t0) >= pollingTimeInUsec);

    co_return PLDM_SUCCESS;
}

exec::task<int> SensorManager::getSensorReading(
    std::shared_ptr<NumericSensor> sensor)
{
    if (!sensor)
    {
        lg2::error("Call `getSensorReading` with null `sensor` pointer.");
        co_return PLDM_ERROR_INVALID_DATA;
    }

    auto tid = sensor->tid;
    auto sensorId = sensor->sensorId;
    Request request(sizeof(pldm_msg_hdr) + PLDM_GET_SENSOR_READING_REQ_BYTES);
    auto requestMsg = new (request.data()) pldm_msg;
    auto rc = encode_get_sensor_reading_req(0, sensorId, false, requestMsg);
    if (rc)
    {
        lg2::error(
            "Failed to encode request GetSensorReading for terminus ID {TID}, sensor Id {ID}, error {RC}.",
            "TID", tid, "ID", sensorId, "RC", rc);
        co_return rc;
    }

    if (!getAvailableState(tid))
    {
        lg2::info(
            "Terminus ID {TID} is not available for PLDM request from {NOW}.",
            "TID", tid, "NOW", pldm::utils::getCurrentSystemTime());
        co_await stdexec::just_stopped();
    }

    const pldm_msg* responseMsg = nullptr;
    size_t responseLen = 0;
    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        lg2::error(
            "Failed to send GetSensorReading message for terminus {TID}, sensor Id {ID}, error {RC}",
            "TID", tid, "ID", sensorId, "RC", rc);
        co_return rc;
    }

    if (!isSensorPollTimerRunning(tid))
    {
        co_return PLDM_ERROR;
    }

    uint8_t completionCode = PLDM_SUCCESS;
    uint8_t sensorDataSize = PLDM_SENSOR_DATA_SIZE_SINT32;
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
        lg2::error(
            "Failed to decode response GetSensorReading for terminus ID {TID}, sensor Id {ID}, error {RC}.",
            "TID", tid, "ID", sensorId, "RC", rc);
        sensor->handleErrGetSensorReading();
        co_return rc;
    }

    if (completionCode != PLDM_SUCCESS)
    {
        lg2::error(
            "Error : GetSensorReading for terminus ID {TID}, sensor Id {ID}, complete code {CC}.",
            "TID", tid, "ID", sensorId, "CC", completionCode);
        co_return completionCode;
    }

    double value = std::numeric_limits<double>::quiet_NaN();
    switch (sensorOperationalState)
    {
        case PLDM_SENSOR_ENABLED:
            break;
        case PLDM_SENSOR_DISABLED:
            sensor->updateReading(false, true, value);
            co_return completionCode;
        case PLDM_SENSOR_FAILED:
            sensor->updateReading(true, false, value);
            co_return completionCode;
        case PLDM_SENSOR_UNAVAILABLE:
        default:
            sensor->updateReading(false, false, value);
            co_return completionCode;
    }

    switch (sensorDataSize)
    {
        case PLDM_SENSOR_DATA_SIZE_UINT8:
            value = static_cast<double>(presentReading.value_u8);
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT8:
            value = static_cast<double>(presentReading.value_s8);
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT16:
            value = static_cast<double>(presentReading.value_u16);
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT16:
            value = static_cast<double>(presentReading.value_s16);
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT32:
            value = static_cast<double>(presentReading.value_u32);
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT32:
            value = static_cast<double>(presentReading.value_s32);
            break;
        default:
            value = std::numeric_limits<double>::quiet_NaN();
            break;
    }

    sensor->updateReading(true, true, value);
    co_return completionCode;
}

exec::task<int> SensorManager::enableStateEffecters(
    pldm_tid_t tid, std::shared_ptr<Terminus> terminus)
{
    if (!terminus)
    {
        lg2::error("Call `enableStateEffecters` with null `terminus` pointer.");
        co_return PLDM_ERROR_INVALID_DATA;
    }

    if (!terminus->doesSupportCommand(PLDM_PLATFORM,
                                      PLDM_SET_STATE_EFFECTER_ENABLES))
    {
        co_return PLDM_SUCCESS;
    }

    /* Copy the list, as the terminus may drop its effecters while a command
     * is in flight
     */
    auto stateEffecters = terminus->stateEffecters;
    for (const auto& effecter : stateEffecters)
    {
        if (!effecter || !effecter->requiresEnable() ||
            effecter->enableState != StateEffecter::EnableState::pending)
        {
            continue;
        }

        auto rc = co_await setStateEffecterEnables(effecter);
        if (!isSensorPollTimerRunning(tid))
        {
            co_return PLDM_ERROR;
        }

        if (rc == PLDM_SUCCESS)
        {
            lg2::info(
                "Terminus ID {TID}: Enabled the composite effecters of state effecter {ID}.",
                "TID", tid, "ID", effecter->getEffecterId());
        }
        else
        {
            lg2::error(
                "Failed to enable state effecter {ID} of terminus {TID}, error: {RC}",
                "ID", effecter->getEffecterId(), "TID", tid, "RC", rc);
        }
    }

    co_return PLDM_SUCCESS;
}

exec::task<int> SensorManager::setStateEffecterEnables(
    std::shared_ptr<StateEffecter> effecter)
{
    if (!effecter)
    {
        lg2::error(
            "Call `setStateEffecterEnables` with null `effecter` pointer.");
        co_return PLDM_ERROR_INVALID_DATA;
    }

    auto tid = effecter->getTid();
    auto effecterId = effecter->getEffecterId();
    auto compositeEffecterCount = effecter->getCompositeEffecterCount();
    if (!compositeEffecterCount ||
        compositeEffecterCount >
            PLDM_PLATFORM_SET_STATE_EFFECTER_ENABLES_MAX_COUNT)
    {
        lg2::error(
            "State effecter {ID} of terminus ID {TID} has an out of range composite effecter count {COUNT}.",
            "ID", effecterId, "TID", tid, "COUNT", compositeEffecterCount);
        co_return PLDM_ERROR_INVALID_DATA;
    }

    /* The state effecters are read by polling, so event messages are
     * disabled for every composite effecter.
     */
    pldm_platform_set_state_effecter_enables_req req{};
    req.effecter_id = effecterId;
    req.composite_effecter_count = compositeEffecterCount;
    for (uint8_t offset = 0; offset < compositeEffecterCount; offset++)
    {
        req.op_fields[offset].effecter_operational_state =
            PLDM_PLATFORM_SET_STATE_EFFECTER_ENABLES_ENABLED;
        req.op_fields[offset].event_msg_enable =
            PLDM_PLATFORM_SET_STATE_EFFECTER_ENABLES_DISABLE_EVENTS;
    }

    Request request(sizeof(pldm_msg_hdr) +
                    PLDM_PLATFORM_SET_STATE_EFFECTER_ENABLES_REQ_MAX_BYTES);
    auto requestMsg = new (request.data()) pldm_msg;
    size_t payloadLength = request.size() - sizeof(pldm_msg_hdr);
    auto rc = encode_pldm_platform_set_state_effecter_enables_req(
        0, &req, requestMsg, &payloadLength);
    if (rc)
    {
        lg2::error(
            "Failed to encode request SetStateEffecterEnables for terminus ID {TID}, effecter Id {ID}, error {RC}.",
            "TID", tid, "ID", effecterId, "RC", rc);
        co_return rc;
    }
    request.resize(sizeof(pldm_msg_hdr) + payloadLength);

    if (!getAvailableState(tid))
    {
        lg2::info(
            "Terminus ID {TID} is not available for PLDM request from {NOW}.",
            "TID", tid, "NOW", pldm::utils::getCurrentSystemTime());
        co_await stdexec::just_stopped();
    }

    const pldm_msg* responseMsg = nullptr;
    size_t responseLen = 0;
    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        lg2::error(
            "Failed to send SetStateEffecterEnables message for terminus {TID}, effecter Id {ID}, error {RC}",
            "TID", tid, "ID", effecterId, "RC", rc);
        co_return rc;
    }

    if (!isSensorPollTimerRunning(tid))
    {
        co_return PLDM_ERROR;
    }

    pldm_platform_set_state_effecter_enables_resp resp{};
    rc = decode_pldm_platform_set_state_effecter_enables_resp(
        responseMsg, responseLen, &resp);
    if (rc)
    {
        lg2::error(
            "Failed to decode response SetStateEffecterEnables for terminus ID {TID}, effecter Id {ID}, error {RC}.",
            "TID", tid, "ID", effecterId, "RC", rc);
        co_return rc;
    }

    if (resp.completion_code != PLDM_SUCCESS)
    {
        /* The terminus answered, so the command is not sent for the effecter
         * again. Only a transport failure leaves the effecter pending.
         */
        effecter->enableState = StateEffecter::EnableState::rejected;
        lg2::error(
            "Error : SetStateEffecterEnables for terminus ID {TID}, effecter Id {ID}, complete code {CC}.",
            "TID", tid, "ID", effecterId, "CC", resp.completion_code);
    }
    else
    {
        effecter->enableState = StateEffecter::EnableState::enabled;
    }

    co_return resp.completion_code;
}

} // namespace platform_mc
} // namespace pldm
