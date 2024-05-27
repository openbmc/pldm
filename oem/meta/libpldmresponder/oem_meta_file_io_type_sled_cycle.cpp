#include "oem_meta_file_io_type_sled_cycle.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;
namespace pldm::responder::oem_meta
{

int SledCycleHandler::write(const message& /*unused*/)
{
    pldm::utils::DBusMapping dbusMapping{
        std::string("/xyz/openbmc_project/state/chassis0"),
        "xyz.openbmc_project.State.Chassis", "RequestedPowerTransition",
        "string"};

    try
    {
        dBusIntf->setDbusProperty(
            dbusMapping,
            "xyz.openbmc_project.State.Chassis.Transition.PowerCycle");
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("{FUNC}: Failed to execute Dbus call, ERROR={ERROR}", "FUNC",
              std::string(__func__), "ERROR", e.what());
    }
    catch (const std::exception& e)
    {
        error("{FUNC}: Failed to set Sled Cycle. ERROR={ERROR}", "FUNC",
              std::string(__func__), "ERROR", e.what());
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

} // namespace pldm::responder::oem_meta
