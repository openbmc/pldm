#include "custom_dbus.hpp"

#include "libpldm/state_set.h"

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

void CustomDBus::setOperationalStatus(const std::string& path, uint8_t status)
{
    if (!operationalStatus.contains(path))
    {
        operationalStatus.emplace(
            path, std::make_unique<OperationalStatusIntf>(
                      pldm::utils::DBusHandler::getBus(), path.c_str()));
    }
    if (status == PLDM_STATE_SET_OPERATIONAL_FAULT_STATUS_NORMAL)
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
    if (operationalStatus.contains(path))
    {
        return operationalStatus.at(path)->functional();
    }
    return false;
}

} // namespace dbus
} // namespace pldm
