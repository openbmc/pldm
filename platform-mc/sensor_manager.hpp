#pragma once

#include "common/types.hpp"
#include "terminus_manager.hpp"

#include <libpldm/platform.h>
#include <libpldm/pldm.h>

#include <map>
#include <memory>
#include <optional>
#include <utility>

namespace pldm
{
namespace platform_mc
{

using namespace pldm::pdr;

inline constexpr uint64_t SENSOR_POLLING_TIME = 249;

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

    /** @brief Enable the state effecters of a terminus whose PDR asks the
     *         initialization agent to enable them and which are not enabled
     *         yet
     *
     *  @param[in] tid - Destination TID
     *  @param[in] terminus - the terminus which owns the state effecters
     *  @return coroutine return_value - PLDM_ERROR when the polling of the
     *          terminus stopped, PLDM_SUCCESS otherwise
     */
    exec::task<int> enableStateEffecters(pldm_tid_t tid,
                                         std::shared_ptr<Terminus> terminus);

    /** @brief Sending setStateEffecterEnables command to enable every
     *         composite effecter of the state effecter
     *
     *  @param[in] effecter - the state effecter to be enabled
     *  @return coroutine return_value - PLDM completion code
     */
    exec::task<int> setStateEffecterEnables(
        std::shared_ptr<StateEffecter> effecter);

    /** @brief Read the state effecters of a terminus whose getter is active
     *
     *  An effecter which waits for SetStateEffecterEnables is read once the
     *  command completes.
     *
     *  @param[in] tid - Destination TID
     *  @param[in] terminus - the terminus which owns the state effecters
     *  @return coroutine return_value - PLDM_ERROR when the polling of the
     *          terminus stopped, PLDM_SUCCESS otherwise
     */
    exec::task<int> readStateEffecters(pldm_tid_t tid,
                                       std::shared_ptr<Terminus> terminus);

    /** @brief Sending getStateEffecterStates command for the state effecter
     *
     *  @param[in] effecter - the state effecter to be read
     *  @return coroutine return_value - PLDM completion code
     */
    exec::task<int> getStateEffecterStates(
        std::shared_ptr<StateEffecter> effecter);

    /** @brief Hand the state set property of each exposed effecter offset of
     *         the state effecters of a terminus to its state effecter, so a
     *         D-Bus client can set the state of the entity
     *
     *  @param[in] tid - Destination TID
     */
    void bindStateEffecters(pldm_tid_t tid);

    /** @brief Accept a state which a D-Bus client requests for an effecter
     *         offset and start the setter of the state effecter
     *
     *  @param[in] effecter - the state effecter
     *  @param[in] offset - effecter offset of the composite effecter
     *  @param[in] state - the requested state
     *  @throws sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument
     *          when the state is not a possible state of the effecter offset
     *  @throws sdbusplus::xyz::openbmc_project::Common::Error::Unavailable
     *          when the setter or the getter of the effecter is active, or
     *          the terminus is not available for PLDM requests
     */
    void requestStateEffecterSet(std::shared_ptr<StateEffecter> effecter,
                                 uint8_t offset, uint8_t state);

    /** @brief Sending setStateEffecterStates command to set the state of one
     *         effecter offset, leaving the other effecter offsets unchanged
     *
     *  @param[in] effecter - the state effecter
     *  @param[in] offset - effecter offset of the composite effecter
     *  @param[in] state - the requested state
     *  @return coroutine return_value - PLDM completion code
     */
    exec::task<int> setStateEffecterStates(
        std::shared_ptr<StateEffecter> effecter, uint8_t offset, uint8_t state);

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

    /** @brief The setters of the state effecters in flight */
    exec::async_scope effecterSetScope;

    /** @brief pointer to Manager */
    Manager* manager;
};
} // namespace platform_mc
} // namespace pldm
