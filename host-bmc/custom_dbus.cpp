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

void CustomDBus::setOperationalStatus(const std::string& path, uint8_t status)
{
    if (operationalStatus.find(path) == operationalStatus.end())
    {
        operationalStatus.emplace(
            path, std::make_unique<OperationalStatusIntf>(
                      pldm::utils::DBusHandler::getBus(), path.c_str()));
    }

    if (status == PLDM_OPERATIONAL_NORMAL)
    {
        operationalStatus.at(path)->functional(true);
    }
    else
    {
        operationalStatus.at(path)->functional(false);
    }
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

void CustomDBus::implementLicInterfaces(
    const std::string& path, const uint32_t& authdevno, const std::string& name,
    const std::string& serialno, const uint64_t& exptime,
    const sdbusplus::com::ibm::License::Entry::server::LicenseEntry::Type& type,
    const sdbusplus::com::ibm::License::Entry::server::LicenseEntry::
        AuthorizationType& authtype)
{
    if (codLic.find(path) == codLic.end())
    {
        codLic.emplace(
            path, std::make_unique<LicIntf>(pldm::utils::DBusHandler::getBus(),
                                            path.c_str()));
    }

    codLic.at(path)->authDeviceNumber(authdevno);
    codLic.at(path)->name(name);
    codLic.at(path)->serialNumber(serialno);
    codLic.at(path)->expirationTime(exptime);
    codLic.at(path)->type(type);
    codLic.at(path)->authorizationType(authtype);
}

void CustomDBus::setAvailabilityState(const std::string& path,
                                      const bool& state)
{
    if (availabilityState.find(path) == availabilityState.end())
    {
        availabilityState.emplace(
            path, std::make_unique<AvailabilityIntf>(
                      pldm::utils::DBusHandler::getBus(), path.c_str()));
    }

    availabilityState.at(path)->available(state);
}

} // namespace dbus
} // namespace pldm
