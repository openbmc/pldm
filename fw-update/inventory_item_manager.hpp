#pragma once

#include "update_manager.hpp"
#include "common/types.hpp"
#include "version.hpp"

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Board/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

namespace pldm::fw_update
{

using InventoryItemBoardIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Board>;
using AssociationDefinitionsIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;
using VersionPurpose =
    sdbusplus::server::xyz::openbmc_project::software::Version::VersionPurpose;

using ObjectPath = pldm::dbus::ObjectPath;

class InventoryItemManager
{
  public:
    InventoryItemManager() = default;
    InventoryItemManager(const InventoryItemManager&) = delete;
    InventoryItemManager(InventoryItemManager&&) = delete;
    InventoryItemManager& operator=(const InventoryItemManager&) = delete;
    InventoryItemManager& operator=(InventoryItemManager&&) = delete;
    ~InventoryItemManager() = default;

    void createInventoryItem(const eid& eid,
                             const FirmwareDeviceName& deviceName,
                             const std::string& activeVersion,
                             const std::shared_ptr<UpdateManager>& updateManager = nullptr);

    void refreshInventoryPath(const eid& eid, const InventoryPath& path);

  private:
    std::string getVersionId(const std::string& version);

    void createVersion(const std::string& path, std::string version,
                       VersionPurpose purpose);

    void createAssociation(const std::string& path, const std::string& foward,
                           const std::string& reverse,
                           const std::string& assocEndPoint);

    const ObjectPath getBoardPath(const InventoryPath& path);

    std::map<eid, InventoryPath> inventoryPathMap;

    std::vector<std::unique_ptr<InventoryItemBoardIntf>> inventoryItems;

    std::vector<std::unique_ptr<Version>> softwareVersions;

    std::vector<std::unique_ptr<AssociationDefinitionsIntf>> associations;
};

} // namespace pldm::fw_update
