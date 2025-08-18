#pragma once

#include "common/types.hpp"
#include "firmware_inventory.hpp"

namespace pldm::fw_update
{

using ObjectPath = pldm::dbus::ObjectPath;

ObjectPath getBoardPath(const InventoryPath& path);
long int getRandomId();

class FirmwareInventoryManager
{
  public:
    FirmwareInventoryManager() = delete;
    FirmwareInventoryManager(const FirmwareInventoryManager&) = delete;
    FirmwareInventoryManager(FirmwareInventoryManager&&) = delete;
    FirmwareInventoryManager& operator=(const FirmwareInventoryManager&) =
        delete;
    FirmwareInventoryManager& operator=(FirmwareInventoryManager&&) = delete;
    ~FirmwareInventoryManager() = default;

    explicit FirmwareInventoryManager(const Configurations& configurations) :
        configurations(configurations)
    {}

    void createFirmwareEntry(
        const SoftwareIdentifier& softwareIdentifier,
        const SoftwareName& softwareName, const std::string& activeVersion,
        const Descriptors& descriptors, const ComponentInfo& componentInfo);

    void deleteFirmwareEntry(const pldm::eid& eid);

  private:
    InventoryPath getInventoryPath(const pldm::eid& eid) const;

    std::string generateVersionHash();

    std::map<SoftwareIdentifier, std::unique_ptr<FirmwareInventory>>
        softwareMap;

    const Configurations& configurations;
};

} // namespace pldm::fw_update
