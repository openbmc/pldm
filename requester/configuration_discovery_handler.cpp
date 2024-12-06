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

const std::map<std::string, PLDMDevice>&
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
    constexpr auto pldmDeviceInterface =
        "xyz.openbmc_project.Configuration.PLDMDevice";
    constexpr auto fwInfoInterface =
        "xyz.openbmc_project.Configuration.PLDMDevice.FirmwareInfo";
    std::string inventoryPath;
    std::string associatedService;
    try
    {
        auto response = dBusIntf->getAssociatedSubTree(mctpReactorObjectPath,
                                                       pldmDeviceInterface);
        if (response.empty())
        {
            error("No associated subtree found for path {PATH}", "PATH",
                  mctpReactorObjectPath);
            return;
        }
        inventoryPath = response.begin()->first;
        associatedService = response.begin()->second.begin()->first;
    }
    catch (const std::exception& e)
    {
        error(
            "{FUNC}: Failed to get associated subtree for path {PATH}, {ERROR}",
            "FUNC", std::string(__func__), "PATH", mctpReactorObjectPath,
            "ERROR", e.what());
    }
    debug("Found PLDM device at path {PATH}", "PATH", inventoryPath);
    try
    {
        auto properties = dBusIntf->getDbusPropertiesVariant(
            associatedService.c_str(), inventoryPath.c_str(),
            pldmDeviceInterface);
        if (!properties.contains("Name") || !properties.contains("RanOver"))
        {
            error("PLDM device at path {PATH} is missing Name, VendorIANA, "
                  "CompatibleHardware or RanOver property",
                  "PATH", inventoryPath);
            return;
        }
        auto name =
            std::visit(utils::VariantToStringVisitor(), properties.at("Name"));

        uint64_t address = 0;
        uint8_t bus = 0;
        auto ranOver = std::visit(utils::VariantToStringVisitor(),
                                  properties.at("RanOver"));
        MCTPEndpoint mctpEndpoint;
        if (ranOver == "MCTPI2CTarget" || ranOver == "MCTPI3CTarget")
        {
            auto mctpTargetInterface =
                std::string{"xyz.openbmc_project.Configuration."} + ranOver;
            auto mctpTargetProperties = dBusIntf->getDbusPropertiesVariant(
                associatedService.c_str(), inventoryPath.c_str(),
                mctpTargetInterface.c_str());
            if (!mctpTargetProperties.contains("Address") ||
                !mctpTargetProperties.contains("Bus"))
            {
                error(
                    "MCTP target device {NAME} at path {PATH} is missing Address or Bus property",
                    "NAME", name, "PATH", inventoryPath);
                return;
            }
            if (mctpTargetProperties.contains("Address"))
            {
                // I3C Target has 6-byte address stored as array of uint8_t
                if (ranOver == "MCTPI3CTarget")
                {
                    auto addressArray = std::visit(
                        utils::VariantToIntegralArrayVisitor<uint8_t>(),
                        mctpTargetProperties.at("Address"));
                    for (const auto& element : addressArray)
                    {
                        address = (address << 8) | element;
                    }
                }
                else
                {
                    address =
                        std::visit(utils::VariantToIntegralVisitor<uint8_t>(),
                                   mctpTargetProperties.at("Address"));
                }
            }
            bus = std::visit(utils::VariantToIntegralVisitor<uint8_t>(),
                             mctpTargetProperties.at("Bus"));
            mctpEndpoint = MCTPEndpoint{eidValue, address, bus};
            debug(
                "Found MCTP target device {NAME} at path {PATH}, address {ADDR}, bus {BUS}",
                "NAME", name, "PATH", inventoryPath, "ADDR", address, "BUS",
                bus);
        }
        else
        {
            error("Unsupported PLDM device type {TYPE} at path {PATH}", "TYPE",
                  ranOver, "PATH", inventoryPath);
        }

        std::optional<FirmwareInfo> fwInfo = std::nullopt;
        auto fwInfoProperties = dBusIntf->getDbusPropertiesVariant(
            associatedService.c_str(), inventoryPath.c_str(), fwInfoInterface);
        if (!fwInfoProperties.empty())
        {
            if (!fwInfoProperties.contains("VendorIANA") ||
                !fwInfoProperties.contains("CompatibleHardware"))
            {
                error(
                    "PLDM device at path {PATH} is missing VendorIANA or CompatibleHardware property",
                    "PATH", inventoryPath);
                return;
            }
            auto iana = std::visit(utils::VariantToIntegralVisitor<uint32_t>(),
                                   fwInfoProperties.at("VendorIANA"));
            auto compatibleHardware =
                std::visit(utils::VariantToStringVisitor(),
                           fwInfoProperties.at("CompatibleHardware"));
            fwInfo = FirmwareInfo{iana, compatibleHardware};
            debug(
                "Found firmware info for PLDM device {NAME} at path {PATH}, IANA {IANA}, CompatibleHardware {COMPATIBLEHARDWARE}",
                "NAME", name, "PATH", inventoryPath, "IANA", iana,
                "COMPATIBLEHARDWARE", compatibleHardware);
        }
        else
        {
            debug("No firmware info found for PLDM device {NAME} at path {PATH}",
                 "NAME", name, "PATH", inventoryPath);
        }
        PLDMDevice pldmDevice{name, mctpEndpoint, fwInfo};
        configurations.insert_or_assign(inventoryPath, pldmDevice);
        debug("Added PLDM device {NAME} at path {PATH}", "NAME", name, "PATH",
             inventoryPath);
    }
    catch (const std::exception& e)
    {
        error("Failed to get PLDM device properties at path {PATH}, {ERROR}",
              "PATH", inventoryPath, "ERROR", e.what());
    }
}

void ConfigurationDiscoveryHandler::removeConfigByEid(uint8_t eid)
{
    std::erase_if(configurations, [eid](const auto& configPair) {
        const auto& pldmDevice = configPair.second;
        if (auto ptr = std::get_if<MCTPEndpoint>(&pldmDevice.ranOver))
        {
            return ptr->endpointId == eid;
        }
        return false;
    });
}

} // namespace pldm
