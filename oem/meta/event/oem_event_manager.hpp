#pragma once

#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "oem_event_manager.hpp"
#include "platform-mc/manager.hpp"
#include "requester/handler.hpp"
#include "requester/request.hpp"

#include <libpldm/pldm.h>

namespace pldm
{
namespace oem_meta
{
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
        requester::Handler<requester::Request>* /* handler */,
        pldm::InstanceIdDb& /* instanceIdDb */,
        platform_mc::Manager* platformManager) :
        event(event), manager(platformManager) {};

    /** @brief Handle the polled CPER (0x07, 0xFA) event class.
     *
     *  @param[in] tid - terminus ID
     *  @param[out] eventId - Event ID
     *  @param[in] eventData - event data
     *  @param[in] eventDataSize - size of event data
     *
     *  @return int - PLDM completion code
     */
    int processOemMsgPollEvent(pldm_tid_t tid, uint16_t eventId,
                               const uint8_t* eventData, size_t eventDataSize);

  protected:
    /** @brief reference of main event loop of pldmd, primarily used to schedule
     *  work
     */
    sdeventplus::Event& event;

    /** @brief A Manager interface for calling the hook functions */
    platform_mc::Manager* manager;
};
} // namespace oem_meta
} // namespace pldm
