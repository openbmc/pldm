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
                      std::map<mctp_eid_t, std::shared_ptr<Terminus>>& termini,
                      Manager* manager) :
        SensorManager(event, terminusManager, termini, manager){};

    MOCK_METHOD(void, doSensorPolling, (tid_t tid), (override));
};

} // namespace platform_mc
} // namespace pldm
