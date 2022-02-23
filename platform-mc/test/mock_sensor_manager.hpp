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
                      TerminusManager& terminusManager,
                      TerminiMapper& termini) :
        SensorManager(event, terminusManager, termini){};

    MOCK_METHOD(void, doSensorPolling, (pldm_tid_t tid), (override));
};

} // namespace platform_mc
} // namespace pldm
