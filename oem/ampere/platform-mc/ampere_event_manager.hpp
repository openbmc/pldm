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
    typedef struct
    {
        std::string firstStt;
        std::string completedStt;
        std::string failStt;
    } sensorDescription;

    std::unordered_map<uint8_t, sensorDescription> numericNormalSensorDesTbl = {
        {0x90, {"SECpro booting", "SECpro completed", "SECpro boot failed"}},
        {0x91, {"Mpro booting", "Mpro completed", "Mpro boot failed"}},
        {0x92, {"ATF BL1 booting", "ATF BL1 completed", "ATF BL1 boot failed"}},
        {0x93, {"ATF BL2 booting", "ATF BL2 completed", "ATF BL2 boot failed"}},
        {0x94,
         {"DDR initialization started", "DDR initialization completed",
          "DDR initialization failed"}},
        {0x97,
         {"ATF BL31 booting", "ATF BL31 completed", "ATF BL31 boot failed"}},
        {0x98,
         {"ATF BL32 booting", "ATF BL32 completed", "ATF BL32 boot failed"}}};

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

    /** @brief Adds eventID to overflow event polling queue
     *  @param[in] tid: Terminus tid
     *  @param[in] eventId: Event id
     *  @return None
     */
    int enqueueOverflowEvent(uint8_t tid, uint16_t eventId);

    /** @brief polling all events in each terminus
     */
    int feedCriticalEventCb();

  private:
    int pldmPollForEventMessage(uint8_t TID, uint8_t eventClass,
                                uint16_t eventID, std::vector<uint8_t> data);
    void handleNumericSensorEventSignal();
    /** @brief critical eventID queue */
    std::deque<std::pair<uint8_t, uint16_t>> overflowEventQueue;
};

} // namespace platform_mc

} // namespace pldm
