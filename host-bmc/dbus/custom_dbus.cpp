#include "custom_dbus.hpp"

#include "serialize.hpp"

namespace pldm
{
namespace dbus
{
void CustomDBus::setLocationCode(const std::string& path, std::string value)
{
    if (!location.contains(path))
    {
        location.emplace(path,
                         std::make_unique<LocationIntf>(
                             pldm::utils::DBusHandler::getBus(), path.c_str()));
    }

    location.at(path)->locationCode(value);
}

std::optional<std::string>
    CustomDBus::getLocationCode(const std::string& path) const
{
    if (location.contains(path))
    {
        return location.at(path)->locationCode();
    }

    return std::nullopt;
}

void CustomDBus::implementCableInterface(const std::string& path)
{
    if (!cable.contains(path))
    {
        cable.emplace(
            path, std::make_unique<Cable>(pldm::utils::DBusHandler::getBus(),
                                          path.c_str()));
    }
}

void CustomDBus::implementPCIeSlotInterface(const std::string& path)
{
    if (pcieSlot.find(path) == pcieSlot.end())
    {
        pcieSlot.emplace(
            path, std::make_unique<PCIeSlot>(pldm::utils::DBusHandler::getBus(),
                                             path.c_str()));
    }
}

void CustomDBus::implementPCIeDeviceInterface(const std::string& path)
{
    if (!pcieDevice.contains(path))
    {
        pcieDevice.emplace(
            path, std::make_unique<PCIeDevice>(
                      pldm::utils::DBusHandler::getBus(), path.c_str()));
    }
}

void CustomDBus::implementAssetInterface(const std::string& path)
{
    if (!asset.contains(path))
    {
        asset.emplace(
            path, std::make_unique<Asset>(pldm::utils::DBusHandler::getBus(),
                                          path.c_str()));
    }
}

void CustomDBus::setAssociations(const std::string& path, AssociationsObj assoc)
{
    using PropVariant = sdbusplus::xyz::openbmc_project::Association::server::
        Definitions::PropertiesVariant;

    if (associations.find(path) == associations.end())
    {
        PropVariant value{std::move(assoc)};
        std::map<std::string, PropVariant> properties;
        properties.emplace("Associations", std::move(value));

        associations.emplace(path, std::make_unique<Associations>(
                                       pldm::utils::DBusHandler::getBus(),
                                       path.c_str(), properties));
    }
    else
    {
        // object already created , so just update the associations
        auto currentAssociations = getAssociations(path);
        AssociationsObj newAssociations;

        for (const auto& association : assoc)
        {
            if (std::find(currentAssociations.begin(),
                          currentAssociations.end(),
                          association) != currentAssociations.end())
            {
                // association is present in current associations
                // do nothing
            }
            else
            {
                currentAssociations.push_back(association);
            }
        }

        associations.at(path)->associations(currentAssociations);
    }
}

const AssociationsObj CustomDBus::getAssociations(const std::string& path)
{
    if (associations.find(path) != associations.end())
    {
        return associations.at(path)->associations();
    }
    return {};
}

void CustomDBus::deleteObject(const std::string& path)
{
    if (location.contains(path))
    {
        location.erase(location.find(path));
    }
    if (pcieSlot.contains(path))
    {
        pcieSlot.erase(pcieSlot.find(path));
    }
    if (associations.contains(path))
    {
        associations.erase(associations.find(path));
    }
    if (cable.contains(path))
    {
        cable.erase(cable.find(path));
    }
    if (asset.contains(path))
    {
        asset.erase(asset.find(path));
    }
    if (pcieDevice.contains(path))
    {
        pcieDevice.erase(pcieDevice.find(path));
    }
}

} // namespace dbus
} // namespace pldm
