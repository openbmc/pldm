#include "custom_dbus.hpp"

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

void CustomDBus::implementCpuCoreInterface(const std::string& path)
{
    if (!cpuCore.contains(path))
    {
        cpuCore.emplace(path, std::make_unique<CPUCore>(
                                  pldm::utils::DBusHandler::getBus(), path));
    }
}

void CustomDBus::setMicroCode(const std::string& path, uint32_t value)
{
    if (!cpuCore.contains(path))
    {
        cpuCore.emplace(path, std::make_unique<CPUCore>(
                                  pldm::utils::DBusHandler::getBus(), path));
    }
    cpuCore.at(path)->microcode(value);
}

std::optional<uint32_t> CustomDBus::getMicroCode(const std::string& path) const
{
    if (cpuCore.contains(path))
    {
        return cpuCore.at(path)->microcode();
    }

    return std::nullopt;
}

void CustomDBus::implementCableInterface(const std::string& path)
{
    if (!cable.contains(path))
    {
        cable.emplace(path, std::make_unique<Cable>(
                                pldm::utils::DBusHandler::getBus(), path));
    }
}

void CustomDBus::setCableAttributes(const std::string& path, double length,
                                    const std::string& cableDescription)
{
    if (cable.contains(path))
    {
        cable.at(path)->length(length);
        cable.at(path)->cableTypeDescription(cableDescription);
    }
}

} // namespace dbus
} // namespace pldm
