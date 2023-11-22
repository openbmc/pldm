#pragma once

#include "libpldm/pldm.h"

#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "oem_event_manager.hpp"
#include "platform-mc/manager.hpp"
#include "requester/handler.hpp"

namespace pldm
{
namespace oem
{
using namespace pldm::pdr;

#define NORMAL_EVENT_POLLING_TIME 5000000 // ms

/**
 * @brief OemEventManager
 *
 *
 */
class OemEventManager
{
  public:
    OemEventManager() = delete;
    OemEventManager(const OemEventManager&) = delete;
    OemEventManager(OemEventManager&&) = delete;
    OemEventManager& operator=(const OemEventManager&) = delete;
    OemEventManager& operator=(OemEventManager&&) = delete;
    virtual ~OemEventManager() = default;

    explicit OemEventManager(
        sdeventplus::Event& event,
        [[maybe_unused]] requester::Handler<requester::Request>& handler,
        [[maybe_unused]] pldm::InstanceIdDb& instanceIdDb,
        platform_mc::Manager* manager) :
        event(event), pollingTime(NORMAL_EVENT_POLLING_TIME),
        manager(manager) {};

    exec::task<int> oemPollForPlatformEvent(pldm_tid_t tid);

  protected:
    void pausePolling();

    sdeventplus::Event& event;
    /** @brief sensor polling interval in ms. */
    uint64_t pollingTime;

    std::map<pldm_tid_t, uint64_t> timeStamp;

    platform_mc::Manager* manager;
};
} // namespace oem
} // namespace pldm
