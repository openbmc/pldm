#pragma once

#include "libpldm/base.h"
#include "libpldm/fru.h"
#include "libpldm/platform.h"

#include "common/types.hpp"
#include "common/utils.hpp"
#include "libpldmresponder/event_parser.hpp"
#include "platform-mc/event_manager.hpp"
#include "requester/handler.hpp"

#include <systemd/sd-journal.h>

#include <sdeventplus/event.hpp>
#include <sdeventplus/source/event.hpp>
#include <sdeventplus/utility/timer.hpp>

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace pldm
{

namespace platform_mc
{

class OemEventManager : public pldm::platform_mc::EventManager
{
  public:
    OemEventManager() = delete;
    OemEventManager(const OemEventManager&) = delete;
    OemEventManager(OemEventManager&&) = default;
    OemEventManager& operator=(const OemEventManager&) = delete;
    OemEventManager& operator=(OemEventManager&&) = default;
    ~OemEventManager() = default;

    explicit OemEventManager(
        sdeventplus::Event& event, TerminusManager& terminusManager,
        std::map<tid_t, std::shared_ptr<Terminus>>& termini);

  private:
    int pldmPollForEventMessage(uint8_t TID, uint8_t eventClass,
                                uint16_t eventID, std::vector<uint8_t> data);
};

} // namespace platform_mc

} // namespace pldm
