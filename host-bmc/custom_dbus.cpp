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

} // namespace dbus
} // namespace pldm