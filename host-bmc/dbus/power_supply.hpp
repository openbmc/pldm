#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Inventory/Item/PowerSupply/server.hpp>

#include <string>

namespace pldm
{
namespace dbus
{
using ItemPowerSupply = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::PowerSupply>;

class PowerSupply : public ItemPowerSupply
{
  public:
    PowerSupply() = delete;
    ~PowerSupply() = default;
    PowerSupply(const PowerSupply&) = delete;
    PowerSupply& operator=(const PowerSupply&) = delete;
    PowerSupply(PowerSupply&&) = delete;
    PowerSupply& operator=(PowerSupply&&) = delete;

    PowerSupply(sdbusplus::bus_t& bus, const std::string& objPath) :
        ItemPowerSupply(bus, objPath.c_str())
    {}
};

} // namespace dbus
} // namespace pldm
