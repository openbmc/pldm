#pragma once

#include "common/types.hpp"
#include "device_dedicated_updater.hpp"

namespace pldm::fw_update
{

using Context = sdbusplus::async::context;
using ObjectPath = pldm::dbus::ObjectPath;

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

    void createSoftwareEntry(const SoftwareIdentifier& softwareIdentifier,
                             const SoftwareName& softwareName,
                             const std::string& activeVersion);

    void refreshInventoryPath(const eid& eid, const InventoryPath& path);

  private:
    std::string getVersionId(const std::string& version);

    Context& ctx;

    const ObjectPath getBoardPath(const InventoryPath& path);

    std::map<eid, InventoryPath> inventoryPathMap;

    std::map<SoftwareIdentifier, std::unique_ptr<DeviceDedicatedUpdater>>
        softwareMap;
};

} // namespace pldm::fw_update
