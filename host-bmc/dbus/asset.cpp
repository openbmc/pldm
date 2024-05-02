#include "asset.hpp"

namespace pldm
{
namespace dbus
{

std::string Asset::partNumber(std::string value)
{
    return sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::
        Asset::partNumber(value);
}

} // namespace dbus
} // namespace pldm
