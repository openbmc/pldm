#pragma once

#include "platform-mc/event_manager.hpp"

#include <gmock/gmock.h>

namespace pldm
{
namespace platform_mc
{

class MockEventManager : public EventManager
{
  public:
    MockEventManager(TerminusManager& terminusManager,
                     std::map<mctp_eid_t, std::shared_ptr<Terminus>>& termini) :
        EventManager(terminusManager, termini){};

    MOCK_METHOD(int, createSensorThresholdLogEntry,
                (const std::string& messageID, const std::string& sensorName,
                 const double reading, const double threshold),
                (override));

    MOCK_METHOD(int, processCperEvent,
                (const uint8_t* eventData, size_t eventDataSize), (override));
};

} // namespace platform_mc
} // namespace pldm
