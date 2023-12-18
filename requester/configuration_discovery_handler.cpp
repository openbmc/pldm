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
    constexpr auto inventoryPath = "/xyz/openbmc_project/inventory/";
    constexpr auto depthWithDownstreamDevices = std::ranges::count(
        "/inventory/system/{BOARD_OR_CHASSIS}/{DEVICE}/{DOWNSTREAM_DEVICE}",
        '/');

    const std::vector<std::string> mctpEndpoint = {
        "xyz.openbmc_project.Configuration.MCTPEndpoint"};

    try
    {
        auto response = dBusIntf->getSubtree(
            inventoryPath, depthWithDownstreamDevices, mctpEndpoint);

        for (const auto& [objectPath, serviceMap] : response)
        {
            appendConfigIfEidMatch(std::get<0>(mctpInfo), objectPath,
                                   serviceMap);
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("{FUNC}: Failed to getSubtree with MCTPEndpoint, error={ERROR}",
              "FUNC", std::string(__func__), "ERROR", e.what());
        return;
    }
    catch (const std::exception& e)
    {
        error("{FUNC}: Unpredicted error occured, error={ERROR}", "FUNC",
              std::string(__func__), "ERROR", e.what());
        return;
    }
}

void ConfigurationDiscoveryHandler::appendConfigIfEidMatch(
    uint8_t targetEid, const std::string& configPath,
    const pldm::utils::MapperServiceMap& serviceMap)
{
    if (!configurations.contains(configPath))
    {
        const auto& serviceName = serviceMap.at(0).first;

        /** mctpEndpointInterface should be
         * "xyz.openbmc_project.Configuration.MCTPEndpoint".
         */
        const auto& mctpEndpointInterface = serviceMap.at(0).second.at(0);
        try
        {
            auto response = dBusIntf->getAll(serviceName, configPath,
                                             mctpEndpointInterface);
            appendIfEidMatch(targetEid, configPath,
                             parseMctpEndpointFromResponse(response));
        }
        catch (const sdbusplus::exception_t& e)
        {
            error(
                "{FUNC}: Failed to getAll of interface={INTERFACE} in path={PATH}, error={ERROR}",
                "FUNC", std::string(__func__), "INTERFACE",
                mctpEndpointInterface, "PATH", configPath, "ERROR", e.what());
            return;
        }
        catch (const NoSuchPropertiesException& e)
        {
            error("{FUNC}: Insufficient properties in {PATH}, error={ERROR}",
                  "FUNC", std::string(__func__), "PATH", configPath, "ERROR",
                  e.what());
            return;
        }
        catch (const std::exception& e)
        {
            error("{FUNC}: Unpredicted error occured, error={ERROR}", "FUNC",
                  std::string(__func__), "ERROR", e.what());
            return;
        }
    }
}

MctpEndpoint ConfigurationDiscoveryHandler::parseMctpEndpointFromResponse(
    const pldm::utils::PropertyMap& response)
{
    if (response.contains("Address") && response.contains("Bus") &&
        response.contains("EndpointId") && response.contains("Name"))
    {
        auto address = std::get<uint64_t>(response.at("Address"));
        auto eid = std::get<uint64_t>(response.at("EndpointId"));
        auto bus = std::get<uint64_t>(response.at("Bus"));
        auto componentName = std::get<std::string>(response.at("Name"));
        if (response.contains("Iana"))
        {
            auto iana = std::get<std::string>(response.at("IANA"));
            return MctpEndpoint{address, eid, bus, componentName, iana};
        }

        return MctpEndpoint{address, eid, bus, componentName, std::nullopt};
    }
    else
    {
        std::vector<std::string> missingProperties{};
        if (response.contains("Address"))
            missingProperties.emplace_back("Address");
        if (response.contains("Bus"))
            missingProperties.emplace_back("Bus");
        if (response.contains("EndpointId"))
            missingProperties.emplace_back("EndpointId");
        if (response.contains("Name"))
            missingProperties.emplace_back("Name");

        throw NoSuchPropertiesException(missingProperties);
    }
}

void ConfigurationDiscoveryHandler::appendIfEidMatch(
    uint8_t targetEid, const std::string& configPath,
    const MctpEndpoint& endpoint)
{
    if (endpoint.EndpointId == targetEid)
    {
        configurations.emplace(configPath, endpoint);
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
