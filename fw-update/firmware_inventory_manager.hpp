#pragma once

#include "aggregate_update_manager.hpp"
#include "common/types.hpp"
#include "condition_collector.hpp"
#include "firmware_inventory.hpp"
#include "requester/endpoint_registry.hpp"

class FirmwareInventoryManagerTest;

namespace pldm::fw_update
{

using ObjectPath = pldm::dbus::ObjectPath;
using SoftwareMap =
    std::map<SoftwareIdentifier, std::unique_ptr<FirmwareInventory>>;

/**
 * @brief Get the Board path from Entity Manager for the given inventory path
 * @param[in] path - Inventory path
 * @param[in] handler - D-Bus handler for querying ancestors
 * @return Board path if found, std::nullopt otherwise
 */
std::optional<std::filesystem::path> getBoardPath(
    const pldm::utils::DBusHandler& handler, const InventoryPath& path);

class FirmwareInventoryManager
{
  public:
    friend class ::FirmwareInventoryManagerTest;
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
     * endpoints
     * @param[in] registry - Tells which MCTP endpoint a PeerId stands for
     */
    explicit FirmwareInventoryManager(
        const pldm::utils::DBusHandler* dbusHandler,
        const Configurations& config, AggregateUpdateManager& updateManager,
        const EndpointRegistry& registry) :
        dbusHandler(dbusHandler), configurations(config),
        updateManager(updateManager), registry(registry),
        conditionConfigManager(FW_UPDATE_TARGETS_JSON)
    {}

    /**
     * @brief Creates a firmware inventory entry for the given software
     * identifier
     * @param[in] softwareIdentifier - Software identifier containing the
     *                                 PeerId and component identifier
     * @param[in] softwareName - Name of the firmware device
     * @param[in] activeVersion - Active version of the firmware
     * @param[in] descriptors - Descriptors associated with the firmware
     * @param[in] componentInfo - Component information associated with the
     * firmware
     */
    void createFirmwareEntry(
        const SoftwareIdentifier& softwareIdentifier,
        const SoftwareName& softwareName, const std::string& activeVersion,
        const Descriptors& descriptors, const ComponentInfo& componentInfo,
        std::function<void()> taskCompletionCallback);

    /**
     * @brief Deletes the firmware inventory entries of the given endpoint
     * @param[in] peer - PeerId of the MCTP endpoint for which the firmware
     *                   inventory entries need to be deleted
     */
    void deleteFirmwareEntry(const PeerId& peer);

  private:
    /**
     * @brief Get the inventory path associated with the given endpoint
     * @param[in] peer - PeerId of the MCTP endpoint
     * @return Inventory path if found, std::nullopt otherwise
     */
    std::optional<InventoryPath> getInventoryPath(const PeerId& peer) const;

    /**
     * @brief D-Bus Handler
     */
    const pldm::utils::DBusHandler* dbusHandler;

    /**
     * @brief Map of software identifier to FirmwareInventory instances
     *        This map maintains the firmware inventory entries created for
     *        different firmware components identified by their software
     *        identifiers (PeerId and component identifier).
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

    /**
     * @brief Reference to the aggregate update manager
     */
    AggregateUpdateManager& updateManager;

    /**
     * @brief Tells which MCTP endpoint a PeerId stands for
     */
    const EndpointRegistry& registry;

    ConditionConfigManager conditionConfigManager;
};

} // namespace pldm::fw_update
