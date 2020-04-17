#pragma once

#include "libpldm/requester/pldm.h"

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
     */
    SoftPowerOff();

    /** @brief Is the pldm-softpoweroff has error.
     * if hasError is true, that means the pldm-softpoweroff failed to
     * trigger the host soft off,so the pldm-softpoweroff will exit.
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

    /** @brief Send PLDM Set State Effecter States command and
     * wait the host gracefully shutdown.
     */
    int hostSoftOff(sdeventplus::Event& event);

  private:
    /** @brief Getting the host current state.
     */
    int getHostState();

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
};

} // namespace pldm
