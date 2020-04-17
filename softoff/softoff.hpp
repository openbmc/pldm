#pragma once

#include "libpldm/requester/pldm.h"

namespace pldm
{

/** @class SoftPowerOff
 *  @brief Responsible for coordinating Host SoftPowerOff operation
 */
class SoftPowerOff
{
  public:
    /** @brief Constructs SoftPowerOff object.
     */
    SoftPowerOff();

    /** @brief Is the pldm-softpoweroff has error.
     * if hasError is true, that means the pldm-softpoweroff
     * can't trigger the host soft off,so the pldm-softpoweroff will exit.
     */
    inline auto isHasError()
    {
        return hasError;
    }

  private:
    /** @brief Getting the host current state.
     */
    int getHostState();

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

    /** @brief Send PLDM Set State Effecter States command.
     */
    int setStateEffecterStates();

    /** @brief Get effecterID from PDRs.
     */
    int getEffecterID();

    /** @brief System firmware EntityType and State set id
     */
    constexpr static uint16_t systemFirmwareEntityType = 31;
    constexpr static uint16_t systemFirmwareStateSetID = 129;

    /** @brief Virtual Machine Manager/VMM EntityType and State set id
     */
    constexpr static uint16_t VMMEntityType = 33;
    constexpr static uint16_t VMMStateSetID = 129;

    /** @brief effecterID state and mctpEID
     */
    uint16_t effecterID;
    constexpr static uint8_t gracefulShutdown = 5;
    uint8_t mctpEID;

    /** @brief Failed to send host soft off command flag.
     */
    bool hasError = false;

    /** @brief Is the Virtual Machine Manager/VMM state effecter available.
     */
    bool VMMPdrExist = true;
};

} // namespace pldm
