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

void ConfigurationDiscoveryHandler::updateMctpEndpointAvailability(
    const MctpInfo& /*mctpinfo*/, Availability /*availability*/)
{
    return;
}

const std::map<std::string, MCTPEndpoint>&
    ConfigurationDiscoveryHandler::getConfigurations() const
{
    return configurations;
}

void ConfigurationDiscoveryHandler::searchConfigurationFor(MctpInfo mctpInfo)
{
    const auto eidValue = std::get<eid>(mctpInfo);
    const auto networkId = std::get<NetworkId>(mctpInfo);
    const auto mctpReactorObjectPath =
        std::string{"/au/com/codeconstruct/mctp1/networks/"} +
        std::to_string(networkId) + "/endpoints/" + std::to_string(eidValue) +
        "/configured_by";
    std::string associatedObjPath;
    std::string associatedService;
    std::string associatedInterface;
    std::vector<std::string> interfaceFilter = {
        "xyz.openbmc_project.Configuration.MCTPI2CTarget",
        "xyz.openbmc_project.Configuration.MCTPI3CTarget"};
    std::string subtreePath = "/xyz/openbmc_project/inventory/system";
    try
    {
        //"/{board or chassis type}/{board or chassis}/{device}"
        auto constexpr subTreeDepth = 3;
        auto response = dBusIntf->getAssociatedSubTree(
            mctpReactorObjectPath, subtreePath, subTreeDepth, interfaceFilter);
        if (response.empty())
        {
            error("No associated subtree found for path {PATH}", "PATH",
                  mctpReactorObjectPath);
            return;
        }
        associatedObjPath = response.begin()->first;
        auto associatedServiceProp = response.begin()->second;
        if (associatedServiceProp.empty())
        {
            error("No associated service found for path {PATH}", "PATH",
                  mctpReactorObjectPath);
            return;
        }
        associatedService = associatedServiceProp.begin()->first;
        auto dBusIntfList = associatedServiceProp.begin()->second;
        auto associatedInterfaceItr = std::find_if(
            dBusIntfList.begin(), dBusIntfList.end(),
            [&interfaceFilter](const auto& intf) {
                return std::find(interfaceFilter.begin(), interfaceFilter.end(),
                                 intf) != interfaceFilter.end();
            });
        if (associatedInterfaceItr == dBusIntfList.end())
        {
            error("No associated interface found for path {PATH}", "PATH",
                  mctpReactorObjectPath);
            return;
        }
        associatedInterface = *associatedInterfaceItr;
    }
    catch (const std::exception& e)
    {
        error("Failed to get associated subtree for path {PATH}: {ERROR}",
              "PATH", mctpReactorObjectPath, "ERROR", e);
    }
    try
    {
        auto mctpTargetProperties = dBusIntf->getDbusPropertiesVariant(
            associatedService.c_str(), associatedObjPath.c_str(),
            associatedInterface.c_str());
        auto mctpEndpoint =
            MCTPEndpoint::create(eidValue, networkId, mctpTargetProperties);
        if (!mctpEndpoint)
        {
            error(
                "Failed to create MCTPEndpoint for endpoint ID {EID} and network ID {NETWORK_ID}: {ERROR}",
                "EID", eidValue, "NETWORK_ID", networkId, "ERROR",
                mctpEndpoint.error());
            return;
        }
        configurations.emplace(associatedObjPath, std::move(*mctpEndpoint));
    }
    catch (const std::exception& e)
    {
        error("Failed to get PLDM device properties at path {PATH}: {ERROR}",
              "PATH", associatedObjPath, "ERROR", e);
    }
}

void ConfigurationDiscoveryHandler::removeConfigByEid(uint8_t eidToRemove)
{
    std::erase_if(configurations, [eidToRemove](const auto& config) {
        auto& [__, mctpEndpoint] = config;
        auto eidValue = std::get<eid>(mctpEndpoint);
        return eidValue == eidToRemove;
    });
}

} // namespace pldm
