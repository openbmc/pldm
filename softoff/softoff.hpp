#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/timer.hpp>

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

    /** @brief Parser the json file to get timeout seconds.
     */
    void parserJsonFile();

  private:
    /* @brief sdbusplus handle */
    sdbusplus::bus::bus& bus;

    /** @brief Reference to Timer object */
    phosphor::Timer timer;

    /** @brief Timeout seconds */
    int timeOutSeconds = 2700;
};

} // namespace pldm
