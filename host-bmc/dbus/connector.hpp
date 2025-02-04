#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Inventory/Item/Connector/server.hpp>

#include <string>

namespace pldm
{
namespace dbus
{
using ItemConnector = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Connector>;

class Connector : public ItemConnector
{
  public:
    Connector() = delete;
    ~Connector() = default;
    Connector(const Connector&) = delete;
    Connector& operator=(const Connector&) = delete;
    Connector(Connector&&) = delete;
    Connector& operator=(Connector&&) = delete;

    Connector(sdbusplus::bus_t& bus, const std::string& objPath) :
        ItemConnector(bus, objPath.c_str()), path(objPath)
    {}

  private:
    std::string path;
};

} // namespace dbus
} // namespace pldm
