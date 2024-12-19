#pragma once

#include "aggregate_update_manager.hpp"
#include "code_updater.hpp"
#include "common/types.hpp"
#include "update_manager.hpp"

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

    InventoryItemManager(Context& ctx,
                         AggregateUpdateManager& aggregateUpdateManager) :
        ctx(ctx),
        aggregateUpdateManager(aggregateUpdateManager)
    {}

    void createInventoryItem(const DeviceIdentifier& deviceIdentifier,
                             const FirmwareDeviceName& deviceName,
                             const std::string& activeVersion,
                             DescriptorMap&& descriptorMap,
                             DownstreamDescriptorMap&& downstreamDescriptorMap,
                             ComponentInfoMap&& componentInfoMap);

    void removeInventoryItem(const DeviceIdentifier& deviceIdentifier);

    void removeInventoryItems(const eid& eid);

    void refreshInventoryPath(const eid& eid, const InventoryPath& path);

  private:
    std::string getVersionId(const std::string& version);

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

    AggregateUpdateManager& aggregateUpdateManager;
};

} // namespace pldm::fw_update
