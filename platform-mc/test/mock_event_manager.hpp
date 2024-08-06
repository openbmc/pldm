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
    MockEventManager(TerminusManager& terminusManager, TerminiMapper& termini) :
        EventManager(terminusManager, termini) {};
};

} // namespace platform_mc
} // namespace pldm
