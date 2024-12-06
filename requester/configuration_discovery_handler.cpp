#include "configuration_discovery_handler.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>

PHOSPHOR_LOG2_USING;

namespace pldm
{

void ConfigurationDiscoveryHandler::handleMctpEndpoints(
    const MctpInfos& newMctpInfos)
{
    for (const auto& newMctpInfo : newMctpInfos)
    {
        searchConfigurationFor(newMctpInfo);
    }
}

void ConfigurationDiscoveryHandler::handleRemovedMctpEndpoints(
    const MctpInfos& removedMctpInfos)
{
    for (const auto& removedMctpInfo : removedMctpInfos)
    {
        removeConfigByEid(std::get<0>(removedMctpInfo));
    }
}

std::map<std::string, MctpEndpoint>&
    ConfigurationDiscoveryHandler::getConfigurations()
{
    return configurations;
}

void ConfigurationDiscoveryHandler::searchConfigurationFor(MctpInfo mctpInfo)
{
    std::string mctpReactorObjectPath =
        "/au/com/codeconstruct/mctp1/networks/1/endpoints/";
    constexpr auto associationInterface =
        "xyz.openbmc_project.Association.Definitions";
    uint8_t eid_value = std::get<eid>(mctpInfo);
    mctpReactorObjectPath += std::to_string(eid_value);
    std::string inventoryPath;

    try
    {
        auto associations = dBusIntf->getDbusPropertyVariant(
            mctpReactorObjectPath.c_str(), "Associations",
            associationInterface);
        if (std::get<
                std::vector<std::tuple<std::string, std::string, std::string>>>(
                associations)
                .empty())
        {
            error("No association found for EID {EID}", "EID", eid_value);
        }
        for (const auto& association : std::get<std::vector<
                 std::tuple<std::string, std::string, std::string>>>(
                 associations))
        {
            info("Association found for EID {EID}", "EID", eid_value);
            if (std::get<0>(association) == "configured_by" &&
                std::get<1>(association) == "configures")
            {
                info(
                    "Association found for EID {EID} with inventory path {PATH}",
                    "EID", eid_value, "PATH", std::get<2>(association));
                inventoryPath = std::get<2>(association);
                break;
            }
            else
            {
                error(
                    "Association found for EID {EID} is not valid, because get<0> is {GET0} and get<1> is {GET1}",
                    "EID", eid_value, "GET0", std::get<0>(association), "GET1",
                    std::get<1>(association));
            }
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        error(
            "{FUNC}: Failed to get property of interface={INTERFACE} in path={PATH}, error={ERROR}",
            "FUNC", std::string(__func__), "INTERFACE", associationInterface,
            "PATH", mctpReactorObjectPath, "ERROR", e.what());
    }
    catch (const std::exception& e)
    {
        error("{FUNC}: Unpredicted error occured, error={ERROR}", "FUNC",
              std::string(__func__), "ERROR", e.what());
    }

    constexpr auto entityManagerService = "xyz.openbmc_project.EntityManager";
    constexpr auto PLDMDeviceInterface =
        "xyz.openbmc_project.Configuration.PLDMDevice";

    if (!inventoryPath.empty())
    {
        try
        {
            auto properties = dBusIntf->getAll(
                entityManagerService, inventoryPath, PLDMDeviceInterface);
            auto addressArray =
                std::get<std::vector<uint64_t>>(properties.at("Address"));
            uint64_t address = 0;
            for (const auto& element : addressArray)
            {
                address = (address << 8) | element;
            }
            auto bus = std::get<uint64_t>(properties.at("Bus"));
            auto iana = std::get<std::string>(properties.at("IANA"));
            auto name = std::get<std::string>(properties.at("Name"));

            MctpEndpoint endpoint{address, eid_value, bus, name, iana};
            configurations.emplace(inventoryPath, endpoint);
        }
        catch (const sdbusplus::exception_t& e)
        {
            error(
                "{FUNC}: Failed to get properties of interface={INTERFACE} in path={PATH}, error={ERROR}",
                "FUNC", std::string(__func__), "INTERFACE", PLDMDeviceInterface,
                "PATH", inventoryPath, "ERROR", e.what());
        }
        catch (const std::exception& e)
        {
            error("{FUNC}: Unpredicted error occurred, error={ERROR}", "FUNC",
                  std::string(__func__), "ERROR", e.what());
        }
    }
}

void ConfigurationDiscoveryHandler::removeConfigByEid(uint8_t eid)
{
    for (const auto& [configDbusPath, mctpEndpoint] : configurations)
    {
        if (mctpEndpoint.EndpointId == eid)
        {
            configurations.erase(configDbusPath);
            return;
        }
    }
}

} // namespace pldm
