#pragma once

#include "libpldm/requester/pldm.h"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/timer.hpp>

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
     * if hasError is true, that means the pldm-softpoweroff
     * can't trigger the host soft off,so the pldm-softpoweroff will exit.
     */
    inline auto isError()
    {
        return hasError;
    }

    /** @brief Is the host soft off completed.
     */
    inline auto isCompleted()
    {
        return completed;
    }

  private:
    /** @brief Getting the host current state.
     */
    int getHostState();

    /** @brief Send PLDM Set State Effecter States command.
     */
    int setStateEffecterStates();

    /** @brief Start the timer.
     */
    int startTimer(const std::chrono::microseconds& usec);

    /** @brief Get effecterID from PDRs.
     */
    int getEffecterID();

    /** @brief effecterID and mctpEID
     */
    uint16_t effecterID;
    uint8_t mctpEID;

    /** @brief Failed to send host soft off command flag.
     */
    bool hasError = false;

    /** @brief Host soft off completed flag.
     */
    bool completed = false;

    /** @brief Is the Virtual Machine Manager/VMM state effecter available.
     */
    bool VMMPdrExist = true;

    /* @brief sdbusplus handle */
    sdbusplus::bus::bus& bus;

    /** @brief Reference to Timer object */
    phosphor::Timer timer;
};

} // namespace pldm
