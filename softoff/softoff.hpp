#pragma once

#include <iostream>

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
    PldmSoftPowerOff();

    /** @brief Send PLDM Set State Effecter States command.
     */
    int setStateEffecterStates();
};

} // namespace pldm
