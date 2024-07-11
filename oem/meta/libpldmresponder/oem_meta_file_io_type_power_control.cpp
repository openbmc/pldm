#include "oem_meta_file_io_type_power_control.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;
namespace pldm::responder::oem_meta
{

constexpr auto slotInterface = "xyz.openbmc_project.Inventory.Decorator.Slot";
constexpr auto slotNumberProperty = "SlotNumber";
const std::vector<std::string> slotInterfaces = {slotInterface};

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
            "{FUNC}: Invalid incoming data for controlling power, data size={SIZE}",
            "FUNC", std::string(__func__), "SIZE", data.size());
        return PLDM_ERROR;
    }

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
            dbusMapping.propertyName = "RequestedPowerTransition";
            property =
                "xyz.openbmc_project.State.Chassis.Transition.PowerCycle";
            break;
        case static_cast<uint8_t>(POWER_CONTROL_OPTION::SLOT_12V_CYCLE):
            dbusMapping.objectPath =
                std::string("/xyz/openbmc_project/state/chassis") +
                getSlotNumber();
            dbusMapping.interface = "xyz.openbmc_project.State.Chassis";
            dbusMapping.propertyName = "RequestedPowerTransition";
            property =
                "xyz.openbmc_project.State.Chassis.Transition.PowerCycle";
            break;
        case static_cast<uint8_t>(POWER_CONTROL_OPTION::SLOT_DC_CYCLE):
            dbusMapping.objectPath =
                std::string("/xyz/openbmc_project/state/host") +
                getSlotNumber();
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
                auto method = bus.new_method_call(systemd_busname, systemd_path,
                                                  systemd_interface,
                                                  "StartUnit");
                method.append("pldm_nic_power_cycle@" +
                                  std::to_string(nic_index) + ".service",
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
        error("{FUNC}: Failed to execute Dbus call, ERROR={ERROR}", "FUNC",
              std::string(__func__), "ERROR", e.what());
        return PLDM_ERROR;
    }
    catch (const std::exception& e)
    {
        error("{FUNC}: Failed to control power. ERROR={ERROR}", "FUNC",
              std::string(__func__), "ERROR", e.what());
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

std::string PowerControlHandler::getSlotNumber()
{
    std::string result{};

    try
    {
        auto slotNumber = pldm::utils::DBusHandler().getDbusProperty<uint64_t>(
            getDbusPathOfBoardContainingTheEndpoint(tid).c_str(),
            slotNumberProperty, slotInterface);

        result = std::to_string(slotNumber);
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("{FUNC}: Failed to execute Dbus call, ERROR={ERROR}", "FUNC",
              std::string(__func__), "ERROR", e.what());
    }
    catch (const std::runtime_error& e)
    {
        error("{FUNC}: Runtime error occurred, ERROR={ERROR}", "FUNC",
              std::string(__func__), "ERROR", e.what());
    }
    catch (const std::exception& e)
    {
        error("{FUNC}: Unexpected exception occurred, ERROR={ERROR}", "FUNC",
              std::string(__func__), "ERROR", e.what());
    }

    return result;
}

const std::string
    PowerControlHandler::getDbusPathOfBoardContainingTheEndpoint(uint8_t tid)
{
    std::string result{};

    try
    {
        const auto& configDbusPath = getConfigurationPathByTid(tid);
        const auto response = dBusIntf->getAncestors(configDbusPath.c_str(),
                                                     slotInterfaces);
        if (response.size() != 1)
        {
            error(
                "{FUNC}: Only Board layer should have Decorator.Slot interface, got {SIZE} Dbus Object(s) have interface Decorator.Slot}",
                "FUNC", std::string(__func__), "SIZE", response.size());
            throw std::runtime_error("Invalid Response of GetAncestors");
        }

        result = std::get<0>(response.front());
    }
    catch (const std::runtime_error& e)
    {
        throw e; // Bypass it.
    }
    catch (const sdbusplus::exception_t& e)
    {
        throw std::runtime_error(
            "Failed to get board path containing the endpoint");
    }

    return result;
}

const std::string& PowerControlHandler::getConfigurationPathByTid(uint8_t tid)
{
    for (const auto& [configDbusPath, mctpEndpoint] : configurations)
    {
        if (mctpEndpoint.EndpointId == tid)
        {
            return configDbusPath;
        }
    }

    error("{FUNC}: Failed to get Configuration for TID{TID}", "FUNC",
          std::string(__func__), "TID", tid);
    throw std::runtime_error("Failed to get Configuration");
}

} // namespace pldm::responder::oem_meta
