#include "chassis.hpp"

namespace pldm
{
namespace dbus
{

auto ItemChassis::type() const -> ChassisType
{
    return sdbusplus::xyz::openbmc_project::Inventory::Item::server::Chassis::
        type();
}

auto ItemChassis::type(ChassisType value) -> ChassisType
{
    return sdbusplus::xyz::openbmc_project::Inventory::Item::server::Chassis::
        type(value);
}

} // namespace dbus
} // namespace pldm
