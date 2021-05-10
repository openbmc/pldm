#pragma once

#include "pldmd/dbus_impl_requester.hpp"

#include <vector>

namespace pldm
{
namespace responder
{
namespace platform
{

/** @brief To send BIOS attribute update event
 *
 *  When the attribute value changes for any BIOS attribute, then
 *  PlatformEventMessage command with OEM event type
 *  PLDM_EVENT_TYPE_OEM_EVENT_BIOS_ATTRIBUTE_UPDATE is send to host with the
 *  list of BIOS attribute handles.
 *
 *  @param[in] fd - socket descriptor to communicate to host
 *  @param[in] eid - MCTP EID of host firmware
 *  @param[in] requester - pointer to Requester object
 *  @param[in] handles - List of BIOS attribute handles
 */
int sendBiosAttributeUpdateEvent(int fd, uint8_t eid,
                                 dbus_api::Requester* requester,
                                 const std::vector<uint16_t>& handles);

class Watchdog
{
  public:
    /** @brief To calculate the heartbeat timeout for sending the
     *     SetEventReceiver to host
     *
     *  @return The heartbeat timeout value */
    int calculateHeartbeatTimeOut();

    /** @brief To check if the watchdog app is running
     *
     *  @return the running status of watchdog app
     */
    bool checkIfWatchDogRunning();

    /** @brief Method to reset the Watchdog timer on receiving platform Event
     *  Message for heartbeat elapsed time from Hostboot
     */
    void resetWatchDogTimer();

    /** @brief To extend the watchdog timer on receiving pldmPDRRepoChngEvent
     *  from host
     */
    void extendWatchDogTimer();

  private:
    /** @brief flag to check if the SetEventReceiver is already sent to host */
    bool isSetEventReceiverSent = false;
    /** @brief flag to check if the watchdog app is running */
    bool isWatchDogRunning = false;
};
} // namespace platform

} // namespace responder

} // namespace pldm
