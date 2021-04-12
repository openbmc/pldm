#include "custom_dbus.hpp"

namespace pldm
{
namespace dbus
{

void CustomDBus::updateLocation(const std::string& path, std::string value)
{
    if (location.find(path) == location.end())
    {
        location.emplace(path,
                         std::make_unique<LocationIntf>(
                             pldm::utils::DBusHandler::getBus(), path.c_str()));
    }

    location.at(path)->locationCode(value);
}

void CustomDBus::updateOperationalStatus(const std::string& path,
                                         uint8_t status)
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

void CustomDBus::updateCorePresentStatus(const std::string& path)
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
} // namespace dbus
} // namespace pldm
