#include "file_io_type_power_control.hpp"

#include "oem/meta/utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

PHOSPHOR_LOG2_USING;

using HostState = sdbusplus::common::xyz::openbmc_project::state::Host;

namespace pldm::responder::oem_meta
{

uint8_t power_control_len = 1;

/* The options a host may ask for. Options 0x00 (sled cycle) and 0x03..0x06
 * (NIC power cycle) are deliberately absent: they act on hardware the
 * requester shares with other hosts, and the request says nothing about
 * whether the sender may do that, so they fall through to the default case
 * and are rejected. What is left acts only on the requester's own slot.
 */
enum class POWER_CONTROL_OPTION
{
    SLOT_12V_CYCLE = 0x01,
    SLOT_DC_CYCLE = 0x02,
};

int PowerControlHandler::write(const message& data)
{
    if (data.size() != power_control_len)
    {
        error(
            "Invalid incoming data for controlling power, data size {SIZE} bytes",
            "SIZE", data.size());
        return PLDM_ERROR;
    }

    std::string slotNum = pldm::oem_meta::getSlotNumberStringByTID(tid);
    uint8_t option = data[0];
    pldm::utils::DBusMapping dbusMapping;
    dbusMapping.propertyType = "string";
    std::string property{};
    switch (option)
    {
        case static_cast<uint8_t>(POWER_CONTROL_OPTION::SLOT_12V_CYCLE):
            dbusMapping.objectPath =
                std::string("/xyz/openbmc_project/state/chassis") + slotNum;
            dbusMapping.interface = "xyz.openbmc_project.State.Chassis";
            dbusMapping.propertyName = "RequestedPowerTransition";
            property =
                "xyz.openbmc_project.State.Chassis.Transition.PowerCycle";
            break;
        case static_cast<uint8_t>(POWER_CONTROL_OPTION::SLOT_DC_CYCLE):
            dbusMapping.objectPath =
                std::format("{}/{}{}", HostState::namespace_path::value,
                            HostState::namespace_path::host, slotNum);
            dbusMapping.interface = HostState::interface;
            dbusMapping.propertyName =
                HostState::property_names::requested_host_transition;
            property = "xyz.openbmc_project.State.Host.Transition.Reboot";
            break;
        default:
            error(
                "Refusing power control option {OPTION} from TID {TID}, slot {SLOT}",
                "OPTION", option, "TID", tid, "SLOT", slotNum);
            return PLDM_ERROR;
    }

    try
    {
        dBusIntf->setDbusProperty(dbusMapping, property);
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("Failed to execute Dbus call with error code {ERROR}", "ERROR",
              e);
        return PLDM_ERROR;
    }
    catch (const std::exception& e)
    {
        error("Failed to control power with error code {ERROR}", "ERROR", e);
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

} // namespace pldm::responder::oem_meta
