#pragma once

#include "common/types.hpp"

#include <xyz/openbmc_project/Association/Definitions/aserver.hpp>
#include <xyz/openbmc_project/Inventory/Item/Board/aserver.hpp>
#include <xyz/openbmc_project/Software/Version/aserver.hpp>

namespace pldm::fw_update
{

class InventoryItemManager;
using InventoryItemBoardIntf =
    sdbusplus::aserver::xyz::openbmc_project::inventory::item::Board<
        InventoryItemManager>;
using SoftwareVersionIntf =
    sdbusplus::aserver::xyz::openbmc_project::software::Version<
        InventoryItemManager>;
using AssociationDefinitionsIntf =
    sdbusplus::aserver::xyz::openbmc_project::association::Definitions<
        InventoryItemManager>;
using VersionPurpose = SoftwareVersionIntf::VersionPurpose;

using ObjectPath = pldm::dbus::ObjectPath;
using Context = sdbusplus::async::context;

struct InventoryItemInterfaces
{
    std::unique_ptr<InventoryItemBoardIntf> board;
    std::unique_ptr<SoftwareVersionIntf> version;
    std::unique_ptr<AssociationDefinitionsIntf> association;
};

class InventoryItemManager
{
  public:
    InventoryItemManager() = delete;
    InventoryItemManager(Context& ctx) : ctx(ctx) {}
    InventoryItemManager(const InventoryItemManager&) = delete;
    InventoryItemManager(InventoryItemManager&&) = delete;
    InventoryItemManager& operator=(const InventoryItemManager&) = delete;
    InventoryItemManager& operator=(InventoryItemManager&&) = delete;
    ~InventoryItemManager() = default;

    void createInventoryItem(const DeviceIdentifier& deviceIdentifier,
                             const FirmwareDeviceName& deviceName,
                             const std::string& activeVersion);

    void refreshInventoryPath(const eid& eid, const InventoryPath& path);

  private:
    void createVersion(InventoryItemInterfaces& interfaces,
                       const std::string& path, const std::string& version,
                       VersionPurpose purpose);

    void createAssociation(InventoryItemInterfaces& interfaces,
                           const std::string& path, const std::string& foward,
                           const std::string& reverse,
                           const std::string& assocEndPoint);

    Context& ctx;

    const ObjectPath getBoardPath(const InventoryPath& path);

    std::map<eid, InventoryPath> inventoryPathMap;

    std::map<DeviceIdentifier, InventoryItemInterfaces> interfacesMap;
};

} // namespace pldm::fw_update
