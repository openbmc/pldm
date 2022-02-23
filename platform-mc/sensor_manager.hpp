#pragma once

#include "libpldm/platform.h"
#include "libpldm/requester/pldm.h"

#include "common/types.hpp"
#include "numeric_sensor.hpp"
#include "pldmd/dbus_impl_requester.hpp"
#include "requester/handler.hpp"
#include "terminus.hpp"

namespace pldm
{
namespace platform_mc
{

/**
 * @brief SensorManager
 *
 * This class manages the sensors found in terminus and provids
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
    ~SensorManager() = default;

    explicit SensorManager(
        sdeventplus::Event& event,
        pldm::requester::Handler<pldm::requester::Request>& handler,
        pldm::dbus_api::Requester& requester,
        std::map<mctp_eid_t, std::shared_ptr<Terminus>>& termini) :
        event(event),
        handler(handler), requester(requester), termini(termini),
        pollingTime(SENSOR_POLLING_TIME){};

    /** @brief starting sensor polling task
     */
    void startPolling();

    /** @brief stopping sensor polling task
     */
    void stopPolling();

  private:
    /** @brief polling all sensors in each terminus
     */
    void doSensorPolling();

    /** @brief Sending getNumericSensorReading command for the sensor
     *
     *  @param[in] sensor - the sensor to be updated
     */
    void sendGetNumericSensorReading(std::shared_ptr<NumericSensor> sensor);

    sdeventplus::Event& event;
    pldm::requester::Handler<pldm::requester::Request>& handler;
    pldm::dbus_api::Requester& requester;
    std::map<mctp_eid_t, std::shared_ptr<Terminus>>& termini;

    uint32_t pollingTime;
    std::unique_ptr<phosphor::Timer> sensorPollTimer;
};
} // namespace platform_mc
} // namespace pldm
