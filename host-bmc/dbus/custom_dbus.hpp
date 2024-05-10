#pragma once

#include "asset.hpp"
#include "associations.hpp"
#include "cable.hpp"
#include "common/utils.hpp"
#include "pcie_device.hpp"
#include "pcie_slot.hpp"

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/LocationCode/server.hpp>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace pldm
{
namespace dbus
{
using ObjectPath = std::string;

using LocationIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::Inventory::
                                    Decorator::server::LocationCode>;

/** @class CustomDBus
 *  @brief This is a custom D-Bus object, used to add D-Bus interface and update
 *         the corresponding properties value.
 */
class CustomDBus
{
  private:
    CustomDBus() {}

  public:
    CustomDBus(const CustomDBus&) = delete;
    CustomDBus(CustomDBus&&) = delete;
    CustomDBus& operator=(const CustomDBus&) = delete;
    CustomDBus& operator=(CustomDBus&&) = delete;
    ~CustomDBus() = default;

    static CustomDBus& getCustomDBus()
    {
        static CustomDBus customDBus;
        return customDBus;
    }

  public:
    /** @brief Set the LocationCode property
     *
     *  @param[in] path  - The object path
     *
     *  @param[in] value - The value of the LocationCode property
     */
    void setLocationCode(const std::string& path, std::string value);

    /** @brief Get the LocationCode property
     *
     *  @param[in] path     - The object path
     *
     *  @return std::optional<std::string> - The value of the LocationCode
     *          property
     */
    std::optional<std::string> getLocationCode(const std::string& path) const;

    /** @brief Implement Slot Interface
     *  @param[in] path - the object path
     */
    void implementPCIeSlotInterface(const std::string& path);

    /** @brief Implement Device Interface
     *  @param[in] path - the object path
     */
    void implementPCIeDeviceInterface(const std::string& path);

    /** @brief Implement Cable Interface
     *  @param[in] path - the object path
     */
    void implementCableInterface(const std::string& path);

    /** @brief Implement Asset Interface
     *  @param[in] path - the object path
     */
    void implementAssetInterface(const std::string& path);

    /** @brief Set the Associations property
     *
     *  @param[in] path     - The object path
     *
     *  @param[in] value    - An array of forward, reverse, endpoint tuples
     */
    void setAssociations(const std::string& path, AssociationsObj assoc);

    /** @brief Get the current Associations property
     *
     *  @param[in] path     - The object path
     *
     *  @return current Associations -  An array of forward, reverse, endpoint
     * tuples
     */
    const std::vector<std::tuple<std::string, std::string, std::string>>
        getAssociations(const std::string& path);

    /** @brief Remove all DBus object paths from cache
     *
     *  @param[in] types  - entity type
     */
    void deleteObject(const std::string& path);

  private:
    std::unordered_map<ObjectPath, std::unique_ptr<LocationIntf>> location;
    std::unordered_map<ObjectPath, std::unique_ptr<PCIeDevice>> pcieDevice;
    std::unordered_map<ObjectPath, std::unique_ptr<PCIeSlot>> pcieSlot;
    std::unordered_map<ObjectPath, std::unique_ptr<Cable>> cable;
    std::unordered_map<ObjectPath, std::unique_ptr<Asset>> asset;
    std::unordered_map<ObjectPath, std::unique_ptr<Associations>> associations;
};

} // namespace dbus
} // namespace pldm
