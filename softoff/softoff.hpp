#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <sdbusplus/timer.hpp>

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

    /** @brief Send chassis off command.
     */
    int sendChassisOffcommand();

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

    /** @brief Guard the obmc-chassis-poweroff@.target.
     *         Prevent hard power off during host soft off.
     */
    int guardChassisOff(uint8_t guardState);

    /** @brief Start the timer.
     */
    int startTimer(const std::chrono::microseconds& usec);

    /** @brief Parser the json file to get timeout seconds.
     */
    void parserJsonFile();

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
};

} // namespace pldm
