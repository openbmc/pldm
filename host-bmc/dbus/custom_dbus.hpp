#pragma once

#include "asset.hpp"
#include "availability.hpp"
#include "board.hpp"
#include "cable.hpp"
#include "chassis.hpp"
#include "common/utils.hpp"
#include "connector.hpp"
#include "cpu_core.hpp"
#include "fabric_adapter.hpp"
#include "fan.hpp"
#include "inventory_item.hpp"
#include "motherboard.hpp"
#include "panel.hpp"
#include "pcie_device.hpp"
#include "pcie_slot.hpp"
#include "power_supply.hpp"
#include "vrm.hpp"

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
     *  @param[in] path - The object path
     *
     *  @return std::optional<std::string> - The value of the LocationCode
     *          property
     */
    std::optional<std::string> getLocationCode(const std::string& path) const;
    /** @brief Implement CpuCore Interface
     *
     *  @param[in] path - The object path
     *
     */
    void implementCpuCoreInterface(const std::string& path);
    /** @brief Set the microcode property
     *
     *  @param[in] path   - The object path
     *
     *  @param[in] value  - microcode value
     */
    void setMicroCode(const std::string& path, uint32_t value);

    /** @brief Get the microcode property
     *
     *  @param[in] path   - The object path
     *
     *  @return std::optional<uint32_t> - The value of the microcode value
     */
    std::optional<uint32_t> getMicroCode(const std::string& path) const;

    /** @brief Implement PCIeSlot Interface
     *
     *  @param[in] path - the object path
     */
    void implementPCIeSlotInterface(const std::string& path);

    /** @brief Set the slot type
     *
     *  @param[in] path - the object path
     *  @param[in] slot type - Slot type
     */
    void setSlotType(const std::string& path, const std::string& slotType);

    /** @brief Implement PCIe Device Interface
     *
     *  @param[in] path - the object path
     */
    void implementPCIeDeviceInterface(const std::string& path);

    /** @brief Set PCIe Device Lanes in use property
     *
     *  @param[in] path - the object path
     *  @param[in] lanesInUse - Lanes in use
     *  @param[in] value - Generation in use
     */
    void setPCIeDeviceProps(const std::string& path, size_t lanesInUse,
                            const std::string& value);

    /** @brief Implement PCIe Cable Interface
     *
     *  @param[in] path - the object path
     */
    void implementCableInterface(const std::string& path);

    /** @brief set cable attributes
     *
     *  @param[in] path - the object path
     *  @param[in] length - length of the wire
     *  @param[in] cableDescription - cable details
     */
    void setCableAttributes(const std::string& path, double length,
                            const std::string& cableDescription);
    /** @brief Implement interface for motherboard property
     *
     *  @param[in] path  - The object path
     *
     */
    void implementMotherboardInterface(const std::string& path);

    /** @brief Implement Fan Interface
     *
     *  @param[in] path - The object path
     *
     */
    void implementFanInterface(const std::string& path);

    /** @brief Implement Chassis Interface
     *  @param[in] path - the object path
     */
    void implementChassisInterface(const std::string& path);

    /** @brief Implement PowerSupply Interface
     *
     *  @param[in] path - The object path
     *
     */
    void implementPowerSupplyInterface(const std::string& path);

    /** @brief Implement Connector Interface
     *
     *  @param[in] path - The object path
     *
     */
    void implementConnecterInterface(const std::string& path);

    /** @brief Implement Fabric Adapter Interface
     *
     *  @param[in] path - The object path
     *
     */
    void implementFabricAdapter(const std::string& path);

    /** @brief Implement Board Interface
     *
     *  @param[in] path - The object path
     *
     */
    void implementBoard(const std::string& path);

    /** @brief Implement Asset Interface
     *
     *  @param[in] path - The object path
     *
     */
    void implementAssetInterface(const std::string& path);

    /** @brief Set the availability state property
     *
     *  @param[in] path   - The object path
     *
     *  @param[in] state  - Availability state
     */
    void setAvailabilityState(const std::string& path, const bool& state);

    /** @brief Set the Inventory Item property
     *  @param[in] path - The object path
     *  @param[in] bool - the presence of fru
     */
    void updateItemPresentStatus(const std::string& path, bool isPresent);

    /** @brief Implement Panel Interface
     *
     *  @param[in] path - The object path
     *
     */
    void implementPanelInterface(const std::string& path);

    /** @brief Implement Voltage Regulator Module Interface
     *
     *  @param[in] path - The object path
     *
     */
    void implementVRMInterface(const std::string& path);

  private:
    std::unordered_map<ObjectPath, std::unique_ptr<Asset>> asset;
    std::unordered_map<ObjectPath, std::unique_ptr<Availability>>
        availabilityState;
    std::unordered_map<ObjectPath, std::unique_ptr<LocationIntf>> location;
    std::unordered_map<ObjectPath, std::unique_ptr<InventoryItem>>
        presentStatus;
    std::unordered_map<ObjectPath, std::unique_ptr<CPUCore>> cpuCore;
    std::unordered_map<ObjectPath, std::unique_ptr<ItemChassis>> chassis;
    std::unordered_map<ObjectPath, std::unique_ptr<PCIeDevice>> pcieDevice;
    std::unordered_map<ObjectPath, std::unique_ptr<PCIeSlot>> pcieSlot;
    std::unordered_map<ObjectPath, std::unique_ptr<PowerSupply>> powersupply;
    std::unordered_map<ObjectPath, std::unique_ptr<Board>> board;
    std::unordered_map<ObjectPath, std::unique_ptr<FabricAdapter>>
        fabricAdapter;
    std::unordered_map<ObjectPath, std::unique_ptr<Cable>> cable;
    std::unordered_map<ObjectPath, std::unique_ptr<Motherboard>> motherboard;
    std::unordered_map<ObjectPath, std::unique_ptr<Fan>> fan;
    std::unordered_map<ObjectPath, std::unique_ptr<Connector>> connector;
    std::unordered_map<ObjectPath, std::unique_ptr<Panel>> panel;
    std::unordered_map<ObjectPath, std::unique_ptr<VRM>> vrm;
};

} // namespace dbus
} // namespace pldm
