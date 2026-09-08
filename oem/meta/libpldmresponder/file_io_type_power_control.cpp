#include "file_io_type_power_control.hpp"

#include "oem/meta/utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

PHOSPHOR_LOG2_USING;

using HostState = sdbusplus::common::xyz::openbmc_project::state::Host;

namespace pldm::responder::oem_meta
{

uint8_t power_control_len = 1;
enum class POWER_CONTROL_OPTION
{
    SLED_CYCLE = 0x00,
    SLOT_12V_CYCLE = 0x01,
    SLOT_DC_CYCLE = 0x02,
    NIC0_POWER_CYCLE = 0x03,
    NIC1_POWER_CYCLE = 0x04,
    NIC2_POWER_CYCLE = 0x05,
    NIC3_POWER_CYCLE = 0x06,
};

#ifndef OEM_META_HOST_SLED_CONTROL
namespace
{

/** @brief Tell whether an option disturbs hosts other than the requester.
 *
 *  A sled cycle power-cycles every host in the sled, and a NIC is shared by
 *  more than one host, so either one takes down neighbours of whoever asked.
 *  The slot cycles are self-scoped by contrast: they act on the slot derived
 *  from the sender's TID, so a host can only cycle itself.
 *
 *  @param[in] option - Power control option from the request.
 *  @return true if the option reaches beyond the requester's own slot.
 */
bool disturbsOtherHosts(uint8_t option)
{
    switch (static_cast<POWER_CONTROL_OPTION>(option))
    {
        case POWER_CONTROL_OPTION::SLED_CYCLE:
        case POWER_CONTROL_OPTION::NIC0_POWER_CYCLE:
        case POWER_CONTROL_OPTION::NIC1_POWER_CYCLE:
        case POWER_CONTROL_OPTION::NIC2_POWER_CYCLE:
        case POWER_CONTROL_OPTION::NIC3_POWER_CYCLE:
            return true;
        default:
            return false;
    }
}

} // namespace
#endif

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

#ifndef OEM_META_HOST_SLED_CONTROL
    if (disturbsOtherHosts(option))
    {
        error(
            "Refusing power control option {OPTION} from TID {TID} (slot {SLOT}): the option disrupts other hosts and the request carries nothing that entitles the sender to it. Rebuild with -Doem-meta-host-sled-control=enabled to allow it.",
            "OPTION", option, "TID", tid, "SLOT", slotNum);
        return PLDM_ERROR;
    }
#endif

    pldm::utils::DBusMapping dbusMapping;
    dbusMapping.propertyType = "string";
    std::string property{};
    switch (option)
    {
        case static_cast<uint8_t>(POWER_CONTROL_OPTION::SLED_CYCLE):
            dbusMapping.objectPath =
                std::string("/xyz/openbmc_project/state/chassis0");
            dbusMapping.interface = "xyz.openbmc_project.State.Chassis";
            dbusMapping.propertyName = "RequestedPowerTransition";
            property =
                "xyz.openbmc_project.State.Chassis.Transition.PowerCycle";
            break;
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
        case static_cast<uint8_t>(POWER_CONTROL_OPTION::NIC0_POWER_CYCLE):
        case static_cast<uint8_t>(POWER_CONTROL_OPTION::NIC1_POWER_CYCLE):
        case static_cast<uint8_t>(POWER_CONTROL_OPTION::NIC2_POWER_CYCLE):
        case static_cast<uint8_t>(POWER_CONTROL_OPTION::NIC3_POWER_CYCLE):
        {
            static constexpr auto systemd_busname = "org.freedesktop.systemd1";
            static constexpr auto systemd_path = "/org/freedesktop/systemd1";
            static constexpr auto systemd_interface =
                "org.freedesktop.systemd1.Manager";
            uint8_t nic_index =
                option -
                static_cast<uint8_t>(POWER_CONTROL_OPTION::NIC0_POWER_CYCLE);
            try
            {
                auto& bus = pldm::utils::DBusHandler::getBus();
                auto method =
                    bus.new_method_call(systemd_busname, systemd_path,
                                        systemd_interface, "StartUnit");
                method.append("nic-powercycle@" + std::to_string(nic_index) +
                                  ".service",
                              "replace");
                bus.call_noreply(method);
            }
            catch (const std::exception& e)
            {
                error("Control NIC{NUM} power fail. ERROR={ERROR}", "NUM",
                      nic_index, "ERROR", e);
                return PLDM_ERROR;
            }
            return PLDM_SUCCESS;
        }
        default:
            error("Get invalid power control option, option={OPTION}", "OPTION",
                  option);
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
