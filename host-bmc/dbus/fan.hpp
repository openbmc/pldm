#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Inventory/Item/Fan/server.hpp>

#include <string>

namespace pldm
{
namespace dbus
{
using ItemFan = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Fan>;

class Fan : public ItemFan
{
  public:
    Fan() = delete;
    ~Fan() = default;
    Fan(const Fan&) = delete;
    Fan& operator=(const Fan&) = delete;
    Fan(Fan&&) = delete;
    Fan& operator=(Fan&&) = delete;

    Fan(sdbusplus::bus_t& bus, const std::string& objPath) :
        ItemFan(bus, objPath.c_str())
    {}
};

} // namespace dbus
} // namespace pldm
