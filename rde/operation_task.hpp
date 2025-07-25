#pragma once

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/RDE/OperationTask/server.hpp>

namespace pldm::rde
{
using OperationTaskIface =
    sdbusplus::xyz::openbmc_project::RDE::server::OperationTask;

/**
 * @class OperationTask
 * @brief D-Bus object representing a Redfish operation task
 *
 * This class attaches to the D-Bus at the provided object path,
 * and exposes the `xyz.openbmc_project.RDE.OperationTask` interface.
 *
 * It is based on the auto-generated sdbusplus server bindings.
 */
class OperationTask : public OperationTaskIface
{
  public:
    OperationTask() = delete;
    OperationTask(const OperationTask&) = delete;
    OperationTask& operator=(const OperationTask&) = delete;
    OperationTask(OperationTask&&) = delete;
    OperationTask& operator=(OperationTask&&) = delete;
    ~OperationTask() = default;

    /**
     * @brief Constructor: creates D-Bus OperationTask object
     * @param bus - sdbusplus bus to attach to
     * @param path - D-Bus object path (e.g.
     * /xyz/openbmc_project/RDE/OperationTask/<id>)
     */
    OperationTask(sdbusplus::bus_t& bus, const std::string& path) :
        sdbusplus::xyz::openbmc_project::RDE::server::OperationTask(
            bus, path.c_str()),
        bus_(bus)
    {
        // Emit D-Bus registration
    }

    void cancel() override {}

  private:
    sdbusplus::bus_t& bus_;
};

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
                          const std::string& payload, uint16_t returnCode);

} // namespace pldm::rde
