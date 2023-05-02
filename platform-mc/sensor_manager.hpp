#pragma once

#include "libpldm/platform.h"
#include "libpldm/pldm.h"

#include "common/types.hpp"
#include "requester/handler.hpp"
#include "terminus.hpp"
#include "terminus_manager.hpp"

namespace pldm
{
namespace platform_mc
{

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

    explicit SensorManager(
        sdeventplus::Event& event, TerminusManager& terminusManager,
        std::map<tid_t, std::shared_ptr<Terminus>>& termini, Manager* manager,
        bool verbose = false,
        const std::filesystem::path& configJson = PLDM_T2_CONFIG_JSON);

    /** @brief starting sensor polling task
     */
    void startPolling(tid_t tid);

    /** @brief stopping sensor polling task
     */
    void stopPolling(tid_t tid);

  protected:
    /** @brief start a coroutine for polling all sensors.
     */
    virtual void doSensorPolling(tid_t tid);

    /** @brief polling all sensors in each terminus
     */
    requester::Coroutine doSensorPollingTask(tid_t tid);

    /** @brief Sending getSensorReading command for the sensor
     *
     *  @param[in] sensor - the sensor to be updated
     */
    requester::Coroutine
        getSensorReading(std::shared_ptr<NumericSensor> sensor);

    /** @brief check if numeric sensor is in priority name spaces
     *
     *  @param[in] sensor - the sensor to be checked
     *
     *  @return bool - true:is in priority
     */
    bool isPriority(std::shared_ptr<NumericSensor> sensor);
    sdeventplus::Event& event;

    /** @brief reference of terminusManager */
    TerminusManager& terminusManager;

    /** @brief List of discovered termini */
    std::map<tid_t, std::shared_ptr<Terminus>>& termini;

    /** @brief sensor polling interval in ms. */
    uint32_t pollingTime;

    /** @brief sensor polling timers */
    std::map<tid_t, std::unique_ptr<sdbusplus::Timer>> sensorPollTimers;

    /** @brief coroutine handle of doSensorPollingTasks */
    std::map<tid_t, std::coroutine_handle<>> doSensorPollingTaskHandles;

    /** @brief force stop polling sensors*/
    bool forceStopPolling = false;

    /** @brief verbose tracing flag */
    bool verbose;

    /** @brief priority SensorNameSpace list */
    std::vector<std::string> prioritySensorNameSpaces;

    /** @brief priority sensor list */
    std::map<tid_t, std::vector<std::shared_ptr<NumericSensor>>>
        prioritySensors;

    /** @brief round robin sensor list */
    std::map<tid_t, std::queue<std::shared_ptr<NumericSensor>>>
        roundRobinSensors;

    /** @brief pointer to Manager */
    Manager* manager;
};
} // namespace platform_mc
} // namespace pldm
