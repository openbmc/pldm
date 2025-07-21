#include "file_io_type_power_control.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;
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

int PowerControlHandler::write(const message& data)
{
    if (data.size() != power_control_len)
    {
        error(
            "Invalid incoming data for controlling power, data size {SIZE} bytes",
            "SIZE", data.size());
        return PLDM_ERROR;
    }

    uint64_t slot;
    try
    {
        slot = getSlotNumberByTID(configurations, tid);
    }
    catch (const std::exception& e)
    {
        error(
            "Fail to get the corresponding slot with TID {TID} and error code {ERROR}",
            "TID", tid, "ERROR", e);
        return PLDM_ERROR;
    }

    std::string slotNum = std::to_string(slot);
    uint8_t option = data[0];
    pldm::utils::DBusMapping dbusMapping;
    dbusMapping.propertyType = "string";
    std::string property{};
    switch (option)
    {
        case static_cast<uint8_t>(POWER_CONTROL_OPTION::SLED_CYCLE):
            dbusMapping.objectPath =
                std::string("/xyz/openbmc_project/state/chassis0");
            dbusMapping.interface = "xyz.openbmc_project.State.Chassis";
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
                std::string("/xyz/openbmc_project/state/host") + slotNum;
            dbusMapping.interface = "xyz.openbmc_project.State.Host";
            dbusMapping.propertyName = "RequestedHostTransition";
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
