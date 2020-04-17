#pragma once

#include <iostream>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <sdbusplus/timer.hpp>

#include "libpldm/requester/pldm.h"

constexpr auto ENABLE_GUARD = 1;
constexpr auto DISABLE_GUARD = 0;

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

    /** @brief Is the host soft off completed.
     */
    inline auto iscompleted()
    {
        return completed;
    }

    /** @brief Is the timer timed out.
     */
    inline auto isTimerExpired()
    {
        return timer.isExpired();
    }

    /** @brief Stop the timer.
     */
    inline auto stopTimer()
    {
        return timer.stop();
    }

    /** @brief When host soft off completed, stop the timer and
     *         set the completed to true.
     */
    void hostSoftOffComplete(sdbusplus::message::message& msg);

    /** @brief Start the timer.
     */
    int startTimer(const std::chrono::microseconds& usec);

    /** @brief Get effecterID from PDRs.
     */
    int getEffecterID();

    /** @brief Get PHYP/HOSTBOOT Sensor info from PDRs.
     */
    int getSensorInfo();

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
    uint16_t HostBoot_EntityType = 31;
    uint16_t HostBoot_State_Set_ID = 129;

    /** @brief PHYP EntityType and State set id
     */
    uint16_t PHYP_EntityType = 33;
    uint16_t PHYP_State_Set_ID = 129;

    /** @brief effecterID state and mctpEID and state.
     * state is 5 : Graceful shutdown requested.
     */
    uint16_t effecterID;
    uint8_t state = 5;
    uint8_t mctpEID;

    /** @brief eventState and sensorID and sensorOffset.
     * eventState = 7 : Graceful Shutdown - The software entity has been shut
     * down gracefully
     */
    uint8_t eventState = 7;
    uint8_t sensorID;
    uint8_t sensorOffset;

    /** @brief Failed to send host soft off command flag.
     */
    bool hasError = false;

    /** @brief Host soft off completed flag.
     */
    bool completed = false;

    /** @brief Is the PHYP state effecter available.
     */
    bool PHYP_PDR_EXIST = true;

    /* @brief sdbusplus handle */
    sdbusplus::bus::bus& bus;

    /** @brief Reference to Timer object */
    phosphor::Timer timer;

    /** @brief Timeout seconds
     * The default is 120 min
     */
    int timeOutSeconds = 7200;

    /** @brief Used to subscribe to dbus pldm StateSensorEvent signal
     *When the host soft off is complete, it sends an pldm event Msg to BMC's
     *pldmd, and the pldmd will emit the StateSensorEvent signal.
     **/
    sdbusplus::bus::match_t pldmEventSignal;
};

} // namespace pldm
