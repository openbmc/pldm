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

#define PLDM_AMPERE_CPER_EVENT_CLASS 0xFA

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
        requester::Handler<requester::Request>& /* handler */,
        pldm::InstanceIdDb& /* instanceIdDb */, platform_mc::Manager* manager) :
        event(event), manager(manager) {};

    /** @brief Helper method to process the PLDM CPER event class
     *
     *  @param[in] tid - tid where the event is from
     *  @param[in] eventId - Event ID which is the source of event
     *  @param[in] eventData - CPER event data
     *  @param[in] eventDataSize - event data length
     *
     *  @return PLDM completion code
     */
    int processOemMsgPollEvent(pldm_tid_t tid, uint16_t eventId,
                               const uint8_t* eventData, size_t eventDataSize);

  protected:
    sdeventplus::Event& event;

    platform_mc::Manager* manager;
};
} // namespace oem
} // namespace pldm
