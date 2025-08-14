#pragma once

#include "common/types.hpp"
#include "firmware_inventory.hpp"

namespace pldm::fw_update
{

using ObjectPath = pldm::dbus::ObjectPath;

class FirmwareInventoryManager
{
  public:
    FirmwareInventoryManager() = default;
    FirmwareInventoryManager(const FirmwareInventoryManager&) = delete;
    FirmwareInventoryManager(FirmwareInventoryManager&&) = delete;
    FirmwareInventoryManager& operator=(const FirmwareInventoryManager&) =
        delete;
    FirmwareInventoryManager& operator=(FirmwareInventoryManager&&) = delete;
    ~FirmwareInventoryManager() = default;

    void createFirmwareEntry(
        const SoftwareIdentifier& softwareIdentifier,
        const SoftwareName& softwareName, const std::string& activeVersion,
        const Descriptors& descriptors, const ComponentInfo& componentInfo);

    void deleteFirmwareEntry(const mctp_eid_t& eid);

    void refreshInventoryPath(const eid& eid, const InventoryPath& path);

  private:
    std::string generateVersionHash(const std::string& version);

    const ObjectPath getBoardPath(const InventoryPath& path);

    std::map<eid, InventoryPath> inventoryPathMap;

    std::map<SoftwareIdentifier, std::unique_ptr<FirmwareInventory>>
        softwareMap;
};

} // namespace pldm::fw_update
