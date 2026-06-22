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
     *  callback with the translated success status.
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
     *  Produces <conditionPath>.service or
     *  <conditionPath>@<leaf-arg>.service.
     *
     *  @param[in] conditionPath - Base unit name
     *  @param[in] args - Optional path-like argument used as instance suffix;
     *                    the leaf node is extracted via std::filesystem::path
     *                    filename()
     *
     *  @return Fully qualified service unit name
     *
     *  @throw std::runtime_error if args does not contain a filename
     */
    std::string serviceName(const ConditionPath& conditionPath,
                            const std::string& args) const;

    sdbusplus::bus_t& bus;

    sdbusplus::bus::match_t systemdSignals;

    std::map<sdbusplus::object_path, TaskCallback> taskCallbacks;

    std::vector<sdbusplus::slot_t> asyncSlots;
};

} // namespace pldm::fw_update
