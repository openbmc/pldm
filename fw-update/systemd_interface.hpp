#pragma once

#include "common/types.hpp"
#include "common/utils.hpp"

#include <libpldm/base.h>

#include <sdbusplus/async.hpp>
#include <sdbusplus/server/object.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace pldm::fw_update
{

namespace sdbusRule = sdbusplus::match_rules;

/** @brief Escapes a string so that it is valid inside a systemd unit name.
 *
 *  Applies the escaping rules of systemd-escape(1): '/' becomes '-', characters
 *  outside the set allowed in a unit name become "\xNN" and a leading '.' is
 *  escaped as well. A unit can recover the original string from its escaped
 *  instance name with the "%I" specifier.
 *
 *  @param[in] arg - The string to escape
 *
 *  @return The escaped string
 */
std::string escapeUnitInstance(std::string_view arg);

/** @brief Builds the systemd unit name of a condition.
 *
 *  @p conditionPath is a unit name as configured for a component, so it carries
 *  its own unit suffix. Named arguments are only meaningful for a parameterized
 *  condition, which is identified by the systemd template naming convention
 *  <name>@.service; the arguments then form its escaped instance name. For any
 *  other unit the arguments are ignored.
 *
 *  @param[in] conditionPath - Configured unit name of the condition
 *  @param[in] args - Named arguments for a parameterized condition
 *
 *  @return The unit name to start
 */
std::string conditionUnitName(const ConditionPath& conditionPath,
                              const std::string& args);

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
 *  bound its own execution time, with TimeoutStartSec for a Type=oneshot unit,
 *  for which RuntimeMaxSec is ignored, and with RuntimeMaxSec for a unit that
 *  reaches the active state. systemd then terminates the unit and delivers the
 *  resulting failure state through JobRemoved. Without such a bound a hanging
 *  unit leaves the update waiting indefinitely.
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
     *  @param[in] conditionPath - Configured unit name of the condition
     *  @param[in] args - Named arguments of a parameterized condition
     *  @param[in] taskCallback - Callback consumed by this API and invoked
     *                            with final execution status
     */
    void execute(const ConditionPath& conditionPath, const std::string& args,
                 TaskCallback&& taskCallback);

    /** @brief Handles systemd JobRemoved signals for tracked jobs.
     *
     *  Matches the job path against pending callbacks and invokes the matched
     *  callback with the translated success status. Only a job result of "done"
     *  is reported as success, so a unit terminated on its start or runtime
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

    /** @brief Releases the slots of StartUnit calls that have been answered.
     *
     *  Called from outside the reply handlers, which still run from the very
     *  slot they would otherwise release.
     */
    void releaseCompletedSlots();

    sdbusplus::bus_t& bus;

    sdbusplus::match systemdSignals;

    std::map<sdbusplus::object_path, TaskCallback> taskCallbacks;

    /** @brief Slots of the StartUnit calls issued so far, keyed by slot id
     *
     *  A slot has to outlive the call it belongs to, so an entry is only erased
     *  once its reply has been handled.
     */
    std::map<uint64_t, sdbusplus::slot_t> asyncSlots;

    /** @brief Ids of the entries in asyncSlots whose reply has been handled */
    std::vector<uint64_t> completedSlots;

    /** @brief Id to use for the next entry in asyncSlots */
    uint64_t nextSlotId = 0;
};

} // namespace pldm::fw_update
