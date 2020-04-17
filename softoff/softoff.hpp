#pragma once

#include <iostream>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <sdbusplus/timer.hpp>

constexpr auto HOST_SOFTOFF_MCTP_ID = 0;
constexpr auto HOST_SOFTOFF_EFFECTER_ID = 0;
constexpr auto HOST_SOFTOFF_STATE = 0;

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
    PldmSoftPowerOff();

    /** @brief Send PLDM Set State Effecter States command.
     */
    int setStateEffecterStates();

  private:
    /** @brief Host soft off completed flag.
     */
    bool completed = false;
};

} // namespace pldm
