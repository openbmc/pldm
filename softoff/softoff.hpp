#pragma once

#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>
#include <unistd.h>

#include <CLI/CLI.hpp>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <sdbusplus/timer.hpp>

#include "libpldm/platform.h"
#include "libpldm/requester/pldm.h"

constexpr auto HOST_SOFTOFF_MCTP_ID = 0;
constexpr auto HOST_SOFTOFF_EFFECTER_ID = 0;
constexpr auto HOST_SOFTOFF_STATE = 0;

constexpr auto ENABLE_GUARD = 1;
constexpr auto DISABLE_GUARD = 0;

namespace pldm
{

namespace fs = std::filesystem;

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

    /** @brief Set the mctpEid/effecterId/state value.
     *
     *  @param[in] mctpEid        - PLDM command parameter mctpEid
     *  @param[in] effecterId     - PLDM command parameter effecterId
     *  @param[in] state          - PLDM command parameter state
     */
    int set_mctpEid_effecterId_state(uint8_t mctpEid, uint16_t effecterId,
                                     uint8_t state);

    /** @brief Send PLDM Set State Effecter States command.
     */
    int setStateEffecterStates();

    /** @brief Parser the json file to get timeout seconds.
     */
    int parserJsonFile();

    /** @brief When host soft off completed, stop the timer and
     *         set the completed to true.
     */
    void setHostSoftOffCompleteFlag(sdbusplus::message::message& msg);

  private:
    /** @brief Host soft off completed flag.
     */
    bool completed = false;

    /* @brief sdbusplus handle */
    sdbusplus::bus::bus& bus;

    /** @brief Reference to Timer object */
    phosphor::Timer timer;

    /** @brief Timeout seconds */
    int timeOutSeconds = 2700;

    /** @brief Subscribe to pldm host soft off property transition
     *
     *  When the host soft off is complete, it sends an pldm event Msg to BMC to
     *chassis off, this application will wait for this event and send chassis
     *off command.
     **/
    sdbusplus::bus::match_t hostTransitionMatch;

    /** @brief PLDM command parameter.
     *
     *  @param[in] mctpEid        - PLDM command parameter mctpEid
     *  @param[in] effecterId     - PLDM command parameter effecterId
     *  @param[in] state          - PLDM command parameter state
     */
    uint8_t mctpEid;
    uint16_t effecterId;
    uint8_t state;
};

} // namespace pldm
