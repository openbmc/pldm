#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Inventory/Item/server.hpp>

#include <string>

namespace pldm
{
namespace dbus
{
using ItemIntf = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Inventory::server::Item>;

class InventoryItem : public ItemIntf
{
  public:
    InventoryItem() = delete;
    ~InventoryItem() = default;
    InventoryItem(const InventoryItem&) = delete;
    InventoryItem& operator=(const InventoryItem&) = delete;
    InventoryItem(InventoryItem&&) = default;
    InventoryItem& operator=(InventoryItem&&) = default;

    InventoryItem(sdbusplus::bus::bus& bus, const std::string& objPath) :
        ItemIntf(bus, objPath.c_str()), path(objPath)
    {}

    /** Get value of PrettyName */
    std::string prettyName() const override;

    /** Set value of PrettyName */
    std::string prettyName(std::string value) override;

    /** Get value of Present */
    bool present() const override;

    /** Set value of Present */
    bool present(bool value) override;

  private:
    std::string path;
};

} // namespace dbus
} // namespace pldm
