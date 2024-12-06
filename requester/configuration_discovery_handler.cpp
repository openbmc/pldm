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
    [[maybe_unused]] const MctpInfo& mctpinfo,
    [[maybe_unused]] Availability availability)
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
    const auto mctpReactorObjectPath =
        std::string{"/au/com/codeconstruct/mctp1/networks/1/endpoints/"} +
        std::to_string(eidValue) + "/configured_by";
    std::string associatedObjPath;
    std::string associatedService;
    std::string associatedInterface;
    std::vector<std::string> interfaceFilter = {
        "xyz.openbmc_project.Configuration.MCTPI2CTarget",
        "xyz.openbmc_project.Configuration.MCTPI3CTarget"};
    std::string subtreePath = "/xyz/openbmc_project/inventory/system";
    try
    {
        auto response = dBusIntf->getAssociatedSubTree(
            mctpReactorObjectPath, subtreePath, 3, interfaceFilter);
        if (response.empty())
        {
            error("No associated subtree found for path {PATH}", "PATH",
                  mctpReactorObjectPath);
            return;
        }
        associatedObjPath = response.begin()->first;
        associatedService = response.begin()->second.begin()->first;
        auto dBusIntfList = response.begin()->second.begin()->second;
        for (const auto& intf : dBusIntfList)
        {
            debug("Associated interface for path {PATH} is {INTERFACE}", "PATH",
                  mctpReactorObjectPath, "INTERFACE", intf);
        }
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
        error("Failed to get associated subtree for path {PATH}, {ERROR}",
              "PATH", mctpReactorObjectPath, "ERROR", e);
    }
    try
    {
        auto mctpTargetProperties = dBusIntf->getDbusPropertiesVariant(
            associatedService.c_str(), associatedObjPath.c_str(),
            associatedInterface.c_str());
        if (!mctpTargetProperties.contains("Address") ||
            !mctpTargetProperties.contains("Bus") ||
            !mctpTargetProperties.contains("Name"))
        {
            error("No Address or Bus property found for path {PATH}", "PATH",
                  associatedObjPath);
            return;
        }
        auto address = std::visit(utils::AddressVisitor<uint64_t, uint8_t>(),
                                  mctpTargetProperties.at("Address"));
        auto bus = std::visit(utils::VariantToIntegralVisitor<uint8_t>(),
                              mctpTargetProperties.at("Bus"));
        auto name = std::get<std::string>(mctpTargetProperties.at("Name"));
        configurations.insert_or_assign(
            associatedObjPath, MCTPEndpoint{eidValue, address, bus, name});
        debug(
            "Added MCTP endpoint {EID} with address {ADDRESS}, bus {BUS}, name {NAME}",
            "EID", eidValue, "ADDRESS", address, "BUS", bus, "NAME", name);
    }
    catch (const std::exception& e)
    {
        error("Failed to get PLDM device properties at path {PATH}, {ERROR}",
              "PATH", associatedObjPath, "ERROR", e);
    }
}

void ConfigurationDiscoveryHandler::removeConfigByEid(uint8_t eid)
{
    std::erase_if(configurations, [eid](const auto& config) {
        return config.second.endpointId == eid;
    });
}

} // namespace pldm
