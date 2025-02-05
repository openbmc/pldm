#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Inventory/Item/Vrm/server.hpp>

#include <string>

namespace pldm
{
namespace dbus
{
using ItemVRM = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Vrm>;

class VRM : public ItemVRM
{
  public:
    VRM() = delete;
    ~VRM() = default;
    VRM(const VRM&) = delete;
    VRM& operator=(const VRM&) = delete;
    VRM(VRM&&) = delete;
    VRM& operator=(VRM&&) = delete;

    VRM(sdbusplus::bus::bus& bus, const std::string& objPath) :
        ItemVRM(bus, objPath.c_str()), path(objPath)
    {}

  private:
    std::string path;
};

} // namespace dbus
} // namespace pldm
