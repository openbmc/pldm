#pragma once

#include "aggregate_update_manager.hpp"
#include "code_updater.hpp"
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

struct InventoryItemInterfaces
{
    std::unique_ptr<InventoryItemBoardIntf> board;
    std::unique_ptr<Version> version;
    std::unique_ptr<AssociationDefinitionsIntf> association;
    std::unique_ptr<CodeUpdater> codeUpdater;
};

class InventoryItemManager
{
  public:
    InventoryItemManager() = delete;
    InventoryItemManager(const InventoryItemManager&) = delete;
    InventoryItemManager(InventoryItemManager&&) = delete;
    InventoryItemManager& operator=(const InventoryItemManager&) = delete;
    InventoryItemManager& operator=(InventoryItemManager&&) = delete;
    ~InventoryItemManager() = default;

    InventoryItemManager(AggregateUpdateManager& aggregateUpdateManager) :
        aggregateUpdateManager(aggregateUpdateManager)
    {}

    void createInventoryItem(
        const DeviceIdentifier& deviceIdentifier,
        const FirmwareDeviceName& deviceName, const std::string& activeVersion,
        DescriptorMap&& descriptorMap, ComponentInfoMap&& componentInfoMap);

    void refreshInventoryPath(const eid& eid, const InventoryPath& path);

  private:
#ifdef OPENSSL
    std::string getVersionId(const std::string& version);
#endif

    void createVersion(InventoryItemInterfaces& interfaces,
                       const std::string& path, std::string version,
                       VersionPurpose purpose);

    void createAssociation(InventoryItemInterfaces& interfaces,
                           const std::string& path, const std::string& foward,
                           const std::string& reverse,
                           const std::string& assocEndPoint);

    const ObjectPath getBoardPath(const InventoryPath& path);

    std::map<eid, InventoryPath> inventoryPathMap;

    std::map<DeviceIdentifier, InventoryItemInterfaces> interfacesMap;

    AggregateUpdateManager& aggregateUpdateManager;
};

} // namespace pldm::fw_update
