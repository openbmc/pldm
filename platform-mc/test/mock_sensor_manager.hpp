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
    MockSensorManager(
        sdeventplus::Event& event,
        pldm::requester::Handler<pldm::requester::Request>& handler,
        pldm::dbus_api::Requester& requester,
        std::map<mctp_eid_t, std::shared_ptr<Terminus>>& termini) :
        SensorManager(event, handler, requester, termini){};

    MOCK_METHOD(void, sendGetSensorReading,
                (std::shared_ptr<NumericSensor> sensor), (override));
};

} // namespace platform_mc
} // namespace pldm