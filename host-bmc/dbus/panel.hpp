#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Inventory/Item/Panel/server.hpp>

#include <string>

namespace pldm
{
namespace dbus
{
using ItemPanel = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Panel>;

class Panel : public ItemPanel
{
  public:
    Panel() = delete;
    ~Panel() = default;
    Panel(const Panel&) = delete;
    Panel& operator=(const Panel&) = delete;
    Panel(Panel&&) = delete;
    Panel& operator=(Panel&&) = delete;

    Panel(sdbusplus::bus_t& bus, const std::string& objPath) :
        ItemPanel(bus, objPath.c_str()), path(objPath)
    {}

  private:
    std::string path;
};

} // namespace dbus
} // namespace pldm
