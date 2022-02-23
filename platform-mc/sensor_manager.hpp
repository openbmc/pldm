#pragma once

#include "libpldm/platform.h"
#include "libpldm/requester/pldm.h"

#include "common/types.hpp"
#include "numeric_sensor.hpp"
#include "pldmd/dbus_impl_requester.hpp"
#include "requester/handler.hpp"
#include "terminus.hpp"

using namespace std::chrono;
namespace pldm
{
namespace platform_mc
{

class Manager;

/**
 * @brief SensorManager
 *
 * This class manages the sensors in each discovered terminus and provids
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
        std::vector<std::shared_ptr<Terminus>>& terminuses) :
        event(event),
        handler(handler), requester(requester), terminuses(terminuses),
        pollingTime(milliseconds(5000)){};

    void doSensorPolling();
    void startPolling();
    void stopPolling();
    void sendGetNumericSensorReading(std::shared_ptr<NumericSensor> sensor);

  private:
    sdeventplus::Event& event;
    pldm::requester::Handler<pldm::requester::Request>& handler;
    pldm::dbus_api::Requester& requester;
    std::vector<std::shared_ptr<Terminus>>& terminuses;

    std::chrono::milliseconds pollingTime;
    std::unique_ptr<phosphor::Timer> sensorPollTimer;
};
} // namespace platform_mc
} // namespace pldm