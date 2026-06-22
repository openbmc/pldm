#pragma once

#include "common/types.hpp"
#include "common/utils.hpp"

#include <libpldm/base.h>

#include <sdbusplus/async.hpp>
#include <sdbusplus/server/object.hpp>

#include <functional>
#include <map>

namespace pldm::fw_update
{

namespace sdbusRule = sdbusplus::bus::match::rules;

/** @class SystemdInterface
 *  @brief Starts pre/post update condition units and tracks their completion.
 *
 *  Per the "Executing Pre/Post Update Services" and "Handling Pre/Post Update
 *  Service Completion" sections of the code update design[1], a condition is
 *  executed with the systemd StartUnit method and its completion is observed by
 *  subscribing to the JobRemoved signal of org.freedesktop.systemd1.Manager.
 *  The job result carried by that signal decides whether the update proceeds or
 *  fails.
 *
 *  systemd emits JobRemoved for every job it dequeues, including jobs that are
 *  cancelled, that fail to start, or that are terminated on a timeout, so a
 *  started job always ends with exactly one JobRemoved. This interface
 *  therefore keeps no timer of its own. Following the "Handling Hanging Tasks"
 *  section of the design[1], a condition unit that can hang is expected to
 *  bound itself with RuntimeMaxSec; systemd then terminates the unit and
 *  delivers the resulting failure state through JobRemoved. Without that
 *  setting a hanging unit leaves the update waiting indefinitely.
 *
 *  [1]:
 *  https://github.com/openbmc/docs/blob/master/designs/code-update.md#pre-and-post-update-conditions
 */
class SystemdInterface
{
  public:
    /** @brief Completion callback for condition execution.
     *
     *  The callback receives true when the systemd job result is successful,
     *  otherwise false.
     */
    using TaskCallback = std::function<void(bool)>;

    SystemdInterface() = delete;
    SystemdInterface(const SystemdInterface&) = delete;
    SystemdInterface(SystemdInterface&&) = delete;
    SystemdInterface& operator=(const SystemdInterface&) = delete;
    SystemdInterface& operator=(SystemdInterface&&) = delete;
    ~SystemdInterface() = default;

    /** @brief Returns the singleton SystemdInterface instance.
     *
     *  @param[in] bus - D-Bus connection used to issue systemd calls
     *
     *  @return Reference to the singleton SystemdInterface
     */
    static SystemdInterface& getInstance(sdbusplus::bus_t& bus);

    /** @brief Executes a condition by starting a systemd unit.
     *
     *  If @p conditionPath is empty, the callback is invoked with success.
     *  Otherwise this issues systemd StartUnit asynchronously and invokes
     *  @p taskCallback when the corresponding JobRemoved signal is received.
     *
     *  @param[in] conditionPath - Base unit name (without .service suffix)
     *  @param[in] args - Optional instance argument used to form unit instance
     *  @param[in] taskCallback - Callback consumed by this API and invoked
     *                            with final execution status
     */
    void execute(const ConditionPath& conditionPath, const std::string& args,
                 TaskCallback&& taskCallback);

    /** @brief Handles systemd JobRemoved signals for tracked jobs.
     *
     *  Matches the job path against pending callbacks and invokes the matched
     *  callback with the translated success status. Only a job result of "done"
     *  is reported as success, so a unit terminated on its RuntimeMaxSec
     *  timeout arrives here as a condition failure. Signals for jobs started by
     *  anyone else are ignored.
     *
     *  @param[in] msg - The JobRemoved signal message
     */
    void handleSystemdJobRemoved(sdbusplus::message_t& msg);

  private:
    /** @brief Constructs the SystemdInterface and subscribes to JobRemoved.
     *
     *  @param[in] bus - D-Bus connection used for systemd method calls/signals
     */
    explicit SystemdInterface(sdbusplus::bus_t& bus);

    /** @brief Builds a service unit name from path and optional argument.
     *
     *  Produces <conditionPath>.service, or <conditionPath>@<args>.service when
     *  arguments are configured, in which case the condition unit has to be a
     *  systemd template unit. The argument string is used as given; deriving it
     *  from D-Bus object paths is the responsibility of the caller.
     *
     *  @param[in] conditionPath - Base unit name
     *  @param[in] args - Optional instance argument used as instance suffix
     *
     *  @return Fully qualified service unit name
     */
    std::string serviceName(const ConditionPath& conditionPath,
                            const std::string& args) const;

    sdbusplus::bus_t& bus;

    sdbusplus::bus::match_t systemdSignals;

    std::map<sdbusplus::object_path, TaskCallback> taskCallbacks;

    std::vector<sdbusplus::slot_t> asyncSlots;
};

} // namespace pldm::fw_update
