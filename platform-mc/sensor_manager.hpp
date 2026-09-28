#pragma once

#include "common/types.hpp"
#include "terminus_manager.hpp"

#include <libpldm/platform.h>
#include <libpldm/pldm.h>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace pldm
{
namespace platform_mc
{

using namespace pldm::pdr;

inline constexpr uint64_t SENSOR_POLLING_TIME = 249;

/** @brief Notification fired whenever a numeric sensor reports a valid
 *  (available and functional) reading, regardless of the OEM. `value` is
 *  the same converted reading published to D-Bus, not the raw pre-
 *  conversion value. Consumers identify sensors of interest by the
 *  `(terminusName, sensorName)` pair, since sensor names are only unique
 *  within a terminus. */
using SensorUpdateHandler = std::function<void(
    pldm_tid_t tid, const std::string& terminusName, uint16_t sensorId,
    const std::string& sensorName, double value)>;

/**
 * @brief SensorManager
 *
 * This class manages the sensors found in terminus and provides
 * function calls for other classes to start/stop sensor monitoring.
 *
 */
class SensorManager
{
  public:
    SensorManager() = delete;
    SensorManager(const SensorManager&) = delete;
    SensorManager(SensorManager&&) = delete;
    SensorManager& operator=(const SensorManager&) = delete;
    SensorManager& operator=(SensorManager&&) = delete;
    virtual ~SensorManager() = default;

    explicit SensorManager(sdeventplus::Event& event,
                           TerminusManager& terminusManager,
                           TerminiMapper& termini, Manager* manager);

    /** @brief starting sensor polling task
     */
    void startPolling(pldm_tid_t tid);

    /** @brief Helper function to start sensor polling timer
     */
    void startSensorPollTimer(pldm_tid_t tid);

    /** @brief Helper function to set all terminus sensor as nan when the
     *  terminus is not available for pldm request
     */
    void disableTerminusSensors(pldm_tid_t tid);

    /** @brief stopping sensor polling task
     */
    void stopPolling(pldm_tid_t tid);

    /** @brief Set available state of terminus for pldm request.
     */
    void updateAvailableState(pldm_tid_t tid, Availability state)
    {
        availableState[tid] = state;
    };

    /** @brief Get available state of terminus for pldm request.
     */
    bool getAvailableState(pldm_tid_t tid)
    {
        auto it = availableState.find(tid);
        if (it == availableState.end())
        {
            return false;
        }
        return it->second;
    };

    /** @brief Register a callback invoked after every valid (available and
     *  functional) numeric sensor reading. Lets any code, such as OEM
     *  code, consume sensor values without the sensor handling in
     *  platform-mc needing to know about it or being modified.
     */
    void registerSensorUpdateHandler(SensorUpdateHandler handler)
    {
        sensorUpdateHandlers.emplace_back(std::move(handler));
    }

  protected:
    /** @brief start a coroutine for polling all sensors.
     */
    virtual void doSensorPolling(pldm_tid_t tid);

    /** @brief polling all sensors in each terminus
     *
     *  @param[in] tid - Destination TID
     *  @return coroutine return_value - PLDM completion code
     */
    exec::task<int> doSensorPollingTask(pldm_tid_t tid);

    /** @brief Sending getSensorReading command for the sensor
     *
     *  @param[in] sensor - the sensor to be updated
     *  @return coroutine return_value - PLDM completion code
     */
    exec::task<int> getSensorReading(std::shared_ptr<NumericSensor> sensor);

    /** @brief Check whether the sensor polling timer of a terminus exists and
     *         is still running
     *
     *  @param[in] tid - Destination TID
     *  @return true if the terminus has a running polling timer
     */
    bool isSensorPollTimerRunning(pldm_tid_t tid) const;

    /** @brief Update a sensor's reading and notify registered
     *  sensor update handlers.
     *
     *  @param[in] sensor - the sensor to be updated
     *  @param[in] available - availability state
     *  @param[in] functional - functional state
     *  @param[in] value - new sensor reading
     */
    void updateSensorReading(const std::shared_ptr<NumericSensor>& sensor,
                             bool available, bool functional, double value);

    /** @brief Reference to to PLDM daemon's main event loop.
     */
    sdeventplus::Event& event;

    /** @brief reference of terminusManager */
    TerminusManager& terminusManager;

    /** @brief List of discovered termini */
    TerminiMapper& termini;

    /** @brief sensor polling interval in ms. */
    uint32_t pollingTime;

    /** @brief sensor polling timers */
    std::map<pldm_tid_t, std::unique_ptr<sdbusplus::Timer>> sensorPollTimers;

    /** @brief Shared state for a single doSensorPolling coroutine run.
     *  Heap-allocated so stopPolling() can erase the map entry immediately
     *  while the coroutine keeps the scope alive via its shared_ptr parameter
     *  until final_suspend completes the scope's internal bookkeeping.
     */
    struct PollHandle
    {
        exec::async_scope scope;
        std::optional<int> rcOpt;
    };

    /** @brief coroutine handle of doSensorPollingTasks */
    std::map<pldm_tid_t, std::shared_ptr<PollHandle>>
        doSensorPollingTaskHandles;

    /** @brief Available state for pldm request of terminus */
    std::map<pldm_tid_t, Availability> availableState;

    /** @brief Round robin sensor iter of terminus */
    std::map<pldm_tid_t, SensorID> roundRobinSensorItMap;

    /** @brief pointer to Manager */
    Manager* manager;

    /** @brief Handlers notified after every numeric sensor reading update */
    std::vector<SensorUpdateHandler> sensorUpdateHandlers;
};
} // namespace platform_mc
} // namespace pldm
