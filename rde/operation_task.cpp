#include <libpldm/base.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>

#include <iostream>
#include <map>
#include <string>
#include <variant>

PHOSPHOR_LOG2_USING;

namespace pldm::rde
{

/**
 * @brief Emit the 'TaskUpdated' D-Bus signal.
 *
 * Signal:
 *   - Interface: xyz.openbmc_project.RDE.OperationTask
 *   - Name: TaskUpdated
 *   - Argument: changed (dict[string, variant])
 *       - "payload": string (JSON-formatted)
 *       - "return_code": uint16
 *
 * This function emits a signal when a task's properties change,
 * allowing observers to track lifecycle updates.
 *
 * @param bus sdbusplus bus reference
 * @param path D-Bus object path for this task instance
 * @param payload JSON string describing task changes
 * @param returnCode Numeric status representing outcome
 * @return TASK_SUCCESS if signal is sent successfully, else TASK_ERROR
 */
int emitTaskUpdatedSignal(sdbusplus::bus_t& bus, const std::string& path,
                          const std::string& payload, uint16_t returnCode)
{
    try
    {
        // Create D-Bus signal message
        auto msg = bus.new_signal(path.c_str(),
                                  "xyz.openbmc_project.RDE.OperationTask",
                                  "TaskUpdated");

        // Dictionary to hold changed properties
        std::map<std::string, std::variant<std::string, uint16_t>> changed;
        changed.emplace("payload", payload);
        changed.emplace("return_code", returnCode);

        msg.append(changed);
        msg.signal_send();
        info(
            "RDE: TaskUpdated signal emitted for path={PATH}, return_code={RC}",
            "PATH", path, "RC", returnCode);
    }
    catch (const std::exception& e)
    {
        error(
            "RDE: Failed to emit TaskUpdated signal for path={PATH}, error={ERR}, return_code={RC}",
            "PATH", path, "ERR", e.what(), "RC", returnCode);

        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

} // namespace pldm::rde
