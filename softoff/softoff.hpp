#pragma once

#include <iostream>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/timer.hpp>

#include "libpldm/requester/pldm.h"

namespace pldm
{

/** @class SoftPowerOff
 *  @brief Responsible for coordinating Host SoftPowerOff operation
 */
class PldmSoftPowerOff
{
  public:
    /** @brief Constructs SoftPowerOff object.
     *
     *  @param[in] bus       - system dbus handler
     *  @param[in] event     - sd_event handler
     */
    PldmSoftPowerOff(sdbusplus::bus::bus& bus, sd_event* event);

    /** @brief Send PLDM Set State Effecter States command.
     */
    int setStateEffecterStates();

    /** @brief Start the timer.
     */
    int startTimer(const std::chrono::microseconds& usec);

    /** @brief Get effecterID from PDRs.
     */
    int getEffecterID();

    /** @brief Is the pldm-softpoweroff has error.
     *if hasError is true, that means the pldm-softpoweroff
     *can't trigger the host soft off,so the pldm-softpoweroff will exit.
     */
    inline auto isHasError()
    {
        return hasError;
    }

    /** @brief PLDM send and receive.
     * Use soft_off_pldm_send_recv API instead of pldm_send_recv because
     * there could be a window where host is transitioning from HB to PHYP
     * and pldm_send_recv can hang.
     */
    pldm_requester_rc_t soft_off_pldm_send_recv(mctp_eid_t eid, int mctp_fd,
                                                const uint8_t* pldm_req_msg,
                                                size_t req_msg_len,
                                                uint8_t** pldm_resp_msg,
                                                size_t* resp_msg_len);

  private:
    /** @brief Hostboot EntityType and State set id
     */
    int HostBoot_EntityType = 31;
    int HostBoot_State_Set_ID = 129;

    /** @brief PHYP EntityType and State set id
     */
    int PHYP_EntityType = 33;
    int PHYP_State_Set_ID = 129;

    /** @brief effecterID state and mctpEID
     */
    uint16_t effecterID;
    uint8_t state = 5;
    uint8_t mctpEID;

    /** @brief Failed to send host soft off command flag.
     */
    bool hasError = false;

    /* @brief sdbusplus handle */
    sdbusplus::bus::bus& bus;

    /** @brief Reference to Timer object */
    phosphor::Timer timer;

    /** @brief Timeout seconds
     * The default is 30 min
     */
    int timeOutSeconds = 7200;
};

} // namespace pldm
