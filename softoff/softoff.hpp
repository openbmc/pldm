#pragma once

#include "libpldm/requester/pldm.h"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/timer.hpp>
#include <sdeventplus/event.hpp>
namespace pldm
{

/** @class SoftPowerOff
 *  @brief Responsible for coordinating Host SoftPowerOff operation
 */
class SoftPowerOff
{
  public:
    /** @brief Constructs SoftPowerOff object.
     *
     *  @param[in] bus       - system D-Bus handler
     *  @param[in] event     - sd_event handler
     */
    SoftPowerOff(sdbusplus::bus::bus& bus, sd_event* event);

    /** @brief Is the pldm-softpoweroff has error.
     * if hasError is true, that means the pldm-softpoweroff failed to
     * trigger the host soft off,so the pldm-softpoweroff will exit.
     */
    inline auto isError()
    {
        return hasError;
    }

    /** @brief Is the timer expired.
     */
    inline auto isTimerExpired()
    {
        return timer.isExpired();
    }

    /** @brief Is the host soft off completed.
     */
    inline auto isCompleted()
    {
        return completed;
    }

    /** @brief Is receive the response for the PLDM request msg.
     */
    inline auto isReceiveResponse()
    {
        return responseReceived;
    }

    /** @brief Send PLDM Set State Effecter States command and
     * wait the host gracefully shutdown.
     */
    int hostSoftOff(sdeventplus::Event& event);

  private:
    /** @brief Getting the host current state.
     */
    int getHostState();

    /** @brief Start the timer.
     */
    int startTimer(const std::chrono::microseconds& usec);

    /** @brief Get effecterID from PDRs.
     */
    int getEffecterID();

    /** @brief effecterID
     */
    uint16_t effecterID;

    /** @brief Failed to send host soft off command flag.
     */
    bool hasError = false;

    /** @brief Host soft off completed flag.
     */
    bool completed = false;

    /** @brief The response for the PLDM request msg is received flag.
     */
    bool responseReceived = false;

    /** @brief Is the Virtual Machine Manager/VMM state effecter available.
     */
    bool VMMPdrExist = true;

    /* @brief sdbusplus handle */
    sdbusplus::bus::bus& bus;

    /** @brief Reference to Timer object */
    phosphor::Timer timer;
};

} // namespace pldm
