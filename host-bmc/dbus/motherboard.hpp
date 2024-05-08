#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Inventory/Item/Board/Motherboard/server.hpp>

#include <string>

namespace pldm
{
namespace dbus
{
using ItemMotherboard = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Inventory::Item::Board::server::
        Motherboard>;

/** @class Motherboard
 *  @brief This class is mapped to Inventory.Item.Board.Motherboard properties
 *         in D-Bus interface path.
 */
class Motherboard : public ItemMotherboard
{
  public:
    Motherboard() = delete;
    ~Motherboard() = default;
    Motherboard(const Motherboard&) = delete;
    Motherboard& operator=(const Motherboard&) = delete;
    Motherboard(Motherboard&&) = default;
    Motherboard& operator=(Motherboard&&) = default;

    Motherboard(sdbusplus::bus::bus& bus, const std::string& objPath) :
        ItemMotherboard(bus, objPath.c_str()), path(objPath)
    {}

  private:
    std::string path;
};

} // namespace dbus
} // namespace pldm
