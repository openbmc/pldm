#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>

#include <string>

namespace pldm
{
namespace dbus
{

using ItemAsset = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset>;

class Asset : public ItemAsset
{
  public:
    Asset() = delete;
    ~Asset() = default;
    Asset(const Asset&) = delete;
    Asset& operator=(const Asset&) = delete;
    Asset(Asset&&) = delete;
    Asset& operator=(Asset&&) = delete;

    Asset(sdbusplus::bus_t& bus, const std::string& objPath) :
        ItemAsset(bus, objPath.c_str()), path(objPath)
    {
        // no need to save this in pldm memory
    }

    std::string partNumber(std::string value) override;

  private:
    std::string path;
};

} // namespace dbus
} // namespace pldm
