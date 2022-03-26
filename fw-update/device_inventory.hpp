#pragma once

#include "common/types.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/message.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Common/UUID/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Chassis/server.hpp>

namespace pldm::fw_update::device_inventory
{

using ObjectPath = sdbusplus::message::object_path;

using ChassisIntf =
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Chassis;
using UUIDIntf = sdbusplus::xyz::openbmc_project::Common::server::UUID;
using AssociationIntf =
    sdbusplus::xyz::openbmc_project::Association::server::Definitions;

using Ifaces =
    sdbusplus::server::object::object<ChassisIntf, UUIDIntf, AssociationIntf>;

/** @class Entry
 *
 *  Implementation of device inventory D-Bus object implementing the D-Bus
 *  interfaces.
 *
 *  a) xyz.openbmc_project.Inventory.Item.Chassis
 *  b) xyz.openbmc_project.Common.UUID
 *  c) xyz.openbmc_project.Association.Definitions
 */
class Entry : public Ifaces
{
  public:
    Entry() = delete;
    ~Entry() = default;
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    Entry(Entry&&) = default;
    Entry& operator=(Entry&&) = default;

    /** @brief Constructor
     *
     *  @param[in] bus  - Bus to attach to
     *  @param[in] objPath - D-Bus object path
     *  @param[in] uuid - MCTP UUID
     *  @param[in] assocs - D-Bus associations
     */
    explicit Entry(sdbusplus::bus::bus& bus, const std::string& objPath,
                   const pldm::UUID& uuid, const Associations& assocs);
};

/** @class Manager
 *
 *  Object manager for device inventory objects
 */
class Manager
{
  public:
    Manager() = delete;
    ~Manager() = default;
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager(Manager&&) = default;
    Manager& operator=(Manager&&) = default;

    /** @brief Constructor
     *
     *  @param[in] bus  - Bus to attach to
     *  @param[in] deviceInventoryInfo - Config info for device inventory
     */
    explicit Manager(sdbusplus::bus::bus& bus,
                     const DeviceInventoryInfo& deviceInventoryInfo);

    /** @brief Create device inventory object
     *
     *  @param[in] uuid - MCTP UUID of the device
     *
     *  @return Object path of the device inventory object
     */
    ObjectPath createEntry(const pldm::UUID& uuid);

  private:
    sdbusplus::bus::bus& bus;

    /** @brief Config info for device inventory */
    const DeviceInventoryInfo& deviceInventoryInfo;

    /** @brief Map to store device inventory objects */
    std::map<pldm::UUID, std::unique_ptr<Entry>> deviceEntryMap;
};

} // namespace pldm::fw_update::device_inventory