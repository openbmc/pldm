#pragma once

#include "common/types.hpp"
#include "firmware_inventory.hpp"
#include "aggregate_update_manager.hpp"

namespace pldm::fw_update
{

using ObjectPath = pldm::dbus::ObjectPath;
using SoftwareMap =
    std::map<SoftwareIdentifier, std::unique_ptr<FirmwareInventory>>;

/**
 * @brief Get the Board path from Entity Manager for the given inventory path
 * @param[in] eid - MCTP endpoint ID
 * @return Inventory path if found, std::nullopt otherwise
 */
std::optional<std::filesystem::path> getBoardPath(const InventoryPath& path);

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

    /**
     * @brief Constructor
     * @param[in] configurations - Reference to the EM configurations for MCTP
     *                             endpoints
     */
    explicit FirmwareInventoryManager(const Configurations& configurations,
                                     AggregateUpdateManager& updateManager) :
        configurations(configurations),
        updateManager(updateManager)
    {}

    /**
     * @brief Creates a firmware inventory entry for the given software
     * identifier
     * @param[in] softwareIdentifier - Software identifier containing EID and
     *                                 component identifier
     * @param[in] softwareName - Name of the firmware device
     * @param[in] activeVersion - Active version of the firmware
     * @param[in] descriptors - Descriptors associated with the firmware
     * @param[in] componentInfo - Component information associated with the
     * firmware
     */
    void createFirmwareEntry(
        const SoftwareIdentifier& softwareIdentifier,
        const SoftwareName& softwareName, const std::string& activeVersion,
        const Descriptors& descriptors, const ComponentInfo& componentInfo);

    /**
     * @brief Deletes the firmware inventory entry for the given EID
     * @param[in] eid - MCTP endpoint ID for which the firmware inventory entry
     *                  needs to be deleted
     */
    void deleteFirmwareEntry(const pldm::eid& eid);

  private:
    /**
     * @brief Get the inventory path associated with the given EID
     * @param[in] eid - MCTP endpoint ID
     * @return Inventory path if found, std::nullopt otherwise
     */
    std::optional<InventoryPath> getInventoryPath(const pldm::eid& eid) const;

    /**
     * @brief Get a random ID for firmware inventory entry
     * @return Random ID as long integer
     */
    long int generateSwId();

    /**
     * @brief Map of software identifier to FirmwareInventory instances
     *        This map maintains the firmware inventory entries created for
     *        different firmware components identified by their software
     *        identifiers (EID and component identifier).
     */
    SoftwareMap softwareMap;

    /**
     * @brief Reference to the EM configurations for MCTP endpoints
     *        This is used to retrieve the associated endpoint information
     *        when creating firmware inventory entries.
     *        It is expected that the configurations are provided during
     *        the initialization of the FirmwareInventoryManager.
     */
    const Configurations& configurations;

    AggregateUpdateManager& updateManager;
};

} // namespace pldm::fw_update
