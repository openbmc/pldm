#pragma once

#include "platform-mc/sensor_manager.hpp"

#include <gmock/gmock.h>

namespace pldm
{
namespace platform_mc
{

class MockSensorManager : public SensorManager
{
  public:
    MockSensorManager(sdeventplus::Event& event,
                      TerminusManager& terminusManager, TerminiMapper& termini,
                      Manager* manager) :
        SensorManager(event, terminusManager, termini, manager) {};

    MOCK_METHOD(void, doSensorPolling, (pldm_tid_t tid), (override));

    /** @brief Expose the GetStateSensorReadings coroutine so a test can drive
     *         it with an injected response.
     */
    exec::task<int> callGetStateSensorReadings(
        pldm_tid_t tid, SensorID sensorId,
        const std::vector<std::shared_ptr<StateSensor>>& components)
    {
        return getStateSensorReadings(tid, sensorId, components);
    }
};

} // namespace platform_mc
} // namespace pldm
