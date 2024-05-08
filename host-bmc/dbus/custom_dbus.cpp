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
    if (cpuCore.find(path) == cpuCore.end())
    {
        cpuCore.emplace(
            path, std::make_unique<CPUCore>(pldm::utils::DBusHandler::getBus(),
                                            path.c_str()));
    }
}

void CustomDBus::setMicrocode(const std::string& path, uint32_t value)
{
    if (cpuCore.find(path) == cpuCore.end())
    {
        cpuCore.emplace(
            path, std::make_unique<CPUCore>(pldm::utils::DBusHandler::getBus(),
                                            path.c_str()));
    }
    cpuCore.at(path)->microcode(value);
}

void CustomDBus::implementMotherboardInterface(const std::string& path)
{
    if (motherboard.find(path) == motherboard.end())
    {
        motherboard.emplace(
            path, std::make_unique<Motherboard>(
                      pldm::utils::DBusHandler::getBus(), path.c_str()));
    }
}
} // namespace dbus
} // namespace pldm
