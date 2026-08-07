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

    /* Expose the polling task and the state sensor commands so a test can
     * drive them directly
     */
    using SensorManager::doSensorPollingTask;
    using SensorManager::getStateSensorReadings;
    using SensorManager::setStateSensorEnables;
};

} // namespace platform_mc
} // namespace pldm
