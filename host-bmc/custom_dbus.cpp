#include "custom_dbus.hpp"

namespace pldm
{
namespace dbus
{

void CustomDBus::setLocationCode(const std::string& path, std::string value)
{
    if (location.find(path) == location.end())
    {
        location.emplace(path,
                         std::make_unique<LocationIntf>(
                             pldm::utils::DBusHandler::getBus(), path.c_str()));
    }

    location.at(path)->locationCode(value);
}

std::string CustomDBus::getLocationCode(const std::string& path) const
{
    if (location.find(path) != location.end())
    {
        return location.at(path)->locationCode();
    }

    return {};
}

void CustomDBus::setOperationalStatus(const std::string& path, bool status)
{
    if (operationalStatus.find(path) == operationalStatus.end())
    {
        operationalStatus.emplace(
            path, std::make_unique<OperationalStatusIntf>(
                      pldm::utils::DBusHandler::getBus(), path.c_str()));
    }

    operationalStatus.at(path)->functional(status);
}

bool CustomDBus::getOperationalStatus(const std::string& path) const
{
    if (operationalStatus.find(path) != operationalStatus.end())
    {
        return operationalStatus.at(path)->functional();
    }

    return false;
}

void CustomDBus::updateItemPresentStatus(const std::string& path)
{
    if (presentStatus.find(path) == presentStatus.end())
    {
        presentStatus.emplace(
            path, std::make_unique<ItemIntf>(pldm::utils::DBusHandler::getBus(),
                                             path.c_str()));
    }

    std::filesystem::path ObjectPath(path);

    // Hardcode the present dbus property to true
    presentStatus.at(path)->present(true);

    // Set the pretty name dbus property to the filename
    // form the dbus path object
    presentStatus.at(path)->prettyName(ObjectPath.filename());
}

void CustomDBus::implementCpuCoreInterface(const std::string& path)
{
    if (cpuCore.find(path) == cpuCore.end())
    {
        cpuCore.emplace(
            path, std::make_unique<CoreIntf>(pldm::utils::DBusHandler::getBus(),
                                             path.c_str()));
        implementObjectEnableIface(path);
    }
}

void CustomDBus::implementObjectEnableIface(const std::string& path)
{
    if (_enabledStatus.find(path) == _enabledStatus.end())
    {
        _enabledStatus.emplace(
            path, std::make_unique<EnableIface>(
                      pldm::utils::DBusHandler::getBus(), path.c_str()));
        _enabledStatus.at(path)->enabled(false);
    }
}

void CustomDBus::setAsserted(const std::string& path, bool value)
{
    if (ledGroup.find(path) == ledGroup.end())
    {
        ledGroup.emplace(path,
                         std::make_unique<AssertedIntf>(
                             pldm::utils::DBusHandler::getBus(), path.c_str()));
    }

    ledGroup.at(path)->asserted(value);
}

void CustomDBus::setAssociations(const std::string& path, Associations assoc)
{
    using PropVariant = sdbusplus::xyz::openbmc_project::Association::server::
        Definitions::PropertiesVariant;

    if (associations.find(path) == associations.end())
    {
        PropVariant value{std::move(assoc)};
        std::map<std::string, PropVariant> properties;
        properties.emplace("Associations", std::move(value));

        associations.emplace(path, std::make_unique<AssociationsIntf>(
                                       pldm::utils::DBusHandler::getBus(),
                                       path.c_str(), properties));
    }
}

} // namespace dbus
} // namespace pldm
