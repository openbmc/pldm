#include "oem_meta_file_io_type_power_status.hpp"

#include <phosphor-logging/lg2.hpp>

#include <sstream>
#include <string>

PHOSPHOR_LOG2_USING;
namespace pldm::responder::oem_meta
{

constexpr auto slotInterface = "xyz.openbmc_project.Inventory.Decorator.Slot";
constexpr auto slotNumberProperty = "SlotNumber";
const std::vector<std::string> slotInterfaces = {slotInterface};

int PowerStatusHandler::write(const message& data)
{
    int completionCode = checkDataIntegrity(data);
    if (completionCode != PLDM_SUCCESS)
    {
        error(
            "{FUNC}: Invalid incoming data for setting ACPIPowerStatus, CC={CC}",
            "FUNC", std::string(__func__), "CC", completionCode);
        return completionCode;
    }

    uint8_t ACPIPowerStatusRawData = data.front();

    pldm::utils::DBusMapping dbusMapping{
        std::string("/xyz/openbmc_project/state/host") + getSlotNumber(),
        "xyz.openbmc_project.Control.Power.ACPIPowerState", "SysACPIStatus",
        "string"};

    try
    {
        dBusIntf->setDbusProperty(
            dbusMapping, convertToACPIDbusValue(ACPIPowerStatusRawData));
    }
    catch (const std::invalid_argument& e)
    {
        error(
            "{FUNC}: Unsupport ACPIPowerStatus. raw value={VALUE}, error={ERROR}",
            "FUNC", std::string(__func__), "VALUE", ACPIPowerStatusRawData,
            "ERROR", e.what());
        return PLDM_ERROR_INVALID_DATA;
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("{FUNC}: Failed to execute Dbus call, ERROR={ERROR}", "FUNC",
              std::string(__func__), "ERROR", e.what());
    }
    catch (const std::exception& e)
    {
        error("{FUNC}: Failed to set SysACPIStatus. ERROR={ERROR}", "FUNC",
              std::string(__func__), "ERROR", e.what());
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

std::string PowerStatusHandler::getSlotNumber()
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
    PowerStatusHandler::getDbusPathOfBoardContainingTheEndpoint(uint8_t tid)
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
        error("{FUNC}: Failed to call GetAncestors, ERROR={ERROR}", "FUNC",
              std::string(__func__), "ERROR", e.what());
        throw std::runtime_error(
            "Failed to get board path containing the endpoint");
    }

    return result;
}

const std::string& PowerStatusHandler::getConfigurationPathByTid(uint8_t tid)
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

int PowerStatusHandler::checkDataIntegrity(const message& data)
{
    if (data.empty())
    {
        return PLDM_ERROR_INVALID_DATA;
    }
    else if (data.size() != 1)
    {
        return PLDM_ERROR_INVALID_LENGTH;
    }
    else
    {
        return PLDM_SUCCESS;
    }
}

pldm::utils::PropertyValue
    PowerStatusHandler::convertToACPIDbusValue(uint8_t rawData)
{
    std::string value = "xyz.openbmc_project.Control.Power.ACPIPowerState.ACPI";
    switch (rawData)
    {
        case 0:
            value += ".S0_G0_D0";
            break;
        case 1:
            value += ".S1_D1";
            break;
        case 2:
            value += ".S2_D2";
            break;
        case 3:
            value += ".S3_D3";
            break;
        case 4:
            value += ".S4";
            break;
        case 5:
            value += ".S5_G2";
            break;
        default:
            throw std::invalid_argument("Unsupport status value");
    }

    return pldm::utils::PropertyValue{value};
}

} // namespace pldm::responder::oem_meta
