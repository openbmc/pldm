#pragma once

#include "serialize.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>

#include <string>

namespace pldm
{
namespace dbus
{

using ItemAsset = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset>;

/**
 * @class Asset
 * @brief Dbus support to provide item physical asset attributes
 */
class Asset : public ItemAsset
{
  public:
    Asset() = delete;
    ~Asset() = default;
    Asset(const Asset&) = delete;
    Asset& operator=(const Asset&) = delete;
    Asset(Asset&&) = default;
    Asset& operator=(Asset&&) = default;

    /**
     * @brief Constructor to initialize the Asset object.
     *
     * @param[in] bus - Reference to the sdbusplus bus object.
     * @param[in] objPath - D-Bus object path.
     */
    Asset(sdbusplus::bus::bus& bus, const std::string& objPath) :
        ItemAsset(bus, objPath.c_str()), path(objPath)
    {
        // no need to save this in pldm memory
    }

    /**
     * @brief Override method to set the part number of the asset.
     *
     * @param[in] value - Part Number value to save.
     * @return part number as a string.
     */
    std::string partNumber(std::string value) override;

  private:
    /**
     * @brief The D-Bus object path.
     */
    std::string path;
};

} // namespace dbus
} // namespace pldm
