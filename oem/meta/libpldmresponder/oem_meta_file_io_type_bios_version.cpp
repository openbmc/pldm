#include "oem_meta_file_io_type_bios_version.hpp"

#include <phosphor-logging/lg2.hpp>

#include <sstream>
#include <string>

PHOSPHOR_LOG2_USING;
namespace pldm::responder::oem_meta
{

constexpr auto slotInterface = "xyz.openbmc_project.Inventory.Decorator.Slot";
constexpr auto slotNumberProperty = "SlotNumber";
const std::vector<std::string> slotInterfaces = {slotInterface};

int BIOSVersionHandler::write(const message& data)
{
    int completionCode = checkDataIntegrity(data);
    if (completionCode != PLDM_SUCCESS)
    {
        error("{FUNC}: Invalid incoming data for setting BIOS version, CC={CC}",
              "FUNC", std::string(__func__), "CC", completionCode);
        return completionCode;
    }

    pldm::utils::DBusMapping dbusMapping{
        std::string("/xyz/openbmc_project/software/host") + getSlotNumber() +
            "/bios_version",
        "xyz.openbmc_project.Software.Version", "Version", "string"};

    try
    {
        dBusIntf->setDbusProperty(dbusMapping, convertToBIOSVersionStr(data));
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("{FUNC}: Failed to execute Dbus call, ERROR={ERROR}", "FUNC",
              std::string(__func__), "ERROR", e.what());
    }
    catch (const std::exception& e)
    {
        error("{FUNC}: Failed to set BIOS version. ERROR={ERROR}", "FUNC",
              std::string(__func__), "ERROR", e.what());
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

std::string BIOSVersionHandler::getSlotNumber()
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
    BIOSVersionHandler::getDbusPathOfBoardContainingTheEndpoint(uint8_t tid)
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

const std::string& BIOSVersionHandler::getConfigurationPathByTid(uint8_t tid)
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

int BIOSVersionHandler::checkDataIntegrity(const message& data)
{
    if (data.empty())
    {
        return PLDM_ERROR_INVALID_DATA;
    }
    else
    {
        return PLDM_SUCCESS;
    }
}

pldm::utils::PropertyValue
    BIOSVersionHandler::convertToBIOSVersionStr(const message& data)
{
    std::string biosVersion(data.begin(), data.end());

    return pldm::utils::PropertyValue{biosVersion};
}

} // namespace pldm::responder::oem_meta
