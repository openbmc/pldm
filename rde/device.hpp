#pragma once

#include "device_common.hpp"
#include "dictionary_manager.hpp"
#include "discov_session.hpp"
#include "resource_registry.hpp"
#include "xyz/openbmc_project/Common/UUID/server.hpp"
#include "xyz/openbmc_project/RDE/Device/server.hpp"

#include <libpldm/base.h>

#include <common/instance_id.hpp>
#include <common/types.hpp>
#include <requester/handler.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/source/event.hpp>

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

namespace pldm::rde
{
using VariantValue = std::variant<int64_t, std::string>;
using PropertyMap = std::map<std::string, VariantValue>;
using SchemaResourcesType = std::map<std::string, PropertyMap>;

using EntryIfaces = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::RDE::server::Device,
    sdbusplus::xyz::openbmc_project::Common::server::UUID>;

/**
 * @class Device
 * @brief Represents a Redfish-capable device managed via D-Bus.
 */
class Device : public EntryIfaces, public std::enable_shared_from_this<Device>
{
  public:
    /**
     * @brief Constructor
     * @param[in] bus - The D-Bus bus object
     * @param[in] event         pldmd sd_event loop
     * @param[in] path - The D-Bus object path
     * @param[in] instanceIdDb  Pointer to the instance ID database used for
     *                          PLDM message tracking.
     * @param[in] handler       Pointer to the PLDM request handler for sending
     *                          and receiving messages.

     * @param[in] eid -  MCTP Endpoint ID used in PLDM stack
     * @param[in] tid - Target ID used in PLDM stack.
     * @param[in] uuid - Internal registration identifier.
     * @param[in] pdrPayloads Vector of raw Redfish Resource PDR payloads,
     *           each PDR as a vector of bytes (std::vector<uint8_t>)
     */
    Device(sdbusplus::bus::bus& bus, sdeventplus::Event& event,
           const std::string& path, pldm::InstanceIdDb* instanceIdDb,
           pldm::requester::Handler<pldm::requester::Request>* handler,
           const pldm::eid devEid, const pldm_tid_t tid, const pldm::UUID& uuid,
           const std::vector<std::vector<uint8_t>>& pdrPayloads);

    // Defaulted special member functions
    Device() = delete;
    Device(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(const Device&) = delete;
    Device& operator=(Device&&) = delete;
    virtual ~Device();

    /**
     * @brief Refreshes and updates capability and schema metadata from the
     * device.
     */
    void refreshDeviceInfo() override;

    /**
     * @brief Access the device metadata.
     * @return Reference to metadata.
     */
    Metadata& getMetadata();

    /**
     * @brief Set the device metadata.
     * @param[in] meta Metadata to assign.
     */
    void setMetadata(const Metadata& meta);

    /**
     * @brief Get metadata field by key.
     *
     * This function retrieves an individual metadata field using its string
     * key. Supported types include: std::string, uint8_t, uint16_t, uint32_t,
     * FeatureSupport, and DeviceCapabilities.
     *
     * @param[in] key Metadata field name.
     * @return Variant holding the value of the metadata field.
     */
    MetadataVariant getMetadataField(const std::string& key) const;

    /**
     * @brief Set metadata field by key.
     *
     * This function assigns a value to a metadata field using its string key.
     * Supported types include: std::string, uint8_t, uint16_t, uint32_t,
     * FeatureSupport, and DeviceCapabilities.
     *
     * @param[in] key Metadata field name.
     * @param[in] value Variant containing the new value to set.
     */
    void setMetadataField(const std::string& key, const MetadataVariant& value);

    /**
     * @brief Returns reference to the PLDM instance ID database.
     * @return Reference to pldm::InstanceIdDb
     */
    inline pldm::InstanceIdDb& getInstanceIdDb()
    {
        return *instanceIdDb_;
    }

    /**
     * @brief Returns a pointer to the PLDM request handler.
     * @return Pointer to Handler of PLDM Request
     */
    inline pldm::requester::Handler<pldm::requester::Request>* getHandler()
    {
        return handler_;
    }

    /**
     * @brief Retrieves the Terminus ID (TID) associated with this instance.
     *
     * This method returns the value of the private member variable that stores
     * the PLDM Terminus ID, used to identify the device during communication.
     *
     * @return pldm_tid_t The stored Terminus ID value.
     */
    inline pldm_tid_t getTid() const
    {
        return tid_;
    }

    /**
     * @brief Attempts to update the device state.
     * @param newState The desired DeviceState.
     */
    void updateState(DeviceState newState);

    /**
     * @brief Returns the current device state.
     */
    DeviceState getState() const;

    /**
     * @brief Retrieve a non-owning pointer to the ResourceRegistry.
     *
     * @return Raw pointer to ResourceRegistry, or nullptr if not initialized.
     */
    ResourceRegistry* getRegistry()
    {
        return resourceRegistry_.get();
    }

    /**
     * @brief Retrieve a non-owning pointer to the DictionaryManager.
     *
     * @return Raw pointer to DictionaryManager, or nullptr if not initialized.
     */
    DictionaryManager* getDictionaryManager()
    {
        return dictionaryManager_.get();
    }

    /**
     * @brief Gets the reference to the sdeventplus::Event object.
     *
     * This function returns a reference to the internal sdeventplus::Event
     * instance used by this class. Use this to interact with the underlying
     * event loop.
     *
     * @return Reference to the sdeventplus::Event object.
     */
    sdeventplus::Event& getEvent()
    {
        return event_;
    }

  private:
    /**
     * @brief Constructs schema resource payload based on discovered resources.
     *
     * Iterates through the registry and transforms each ResourceInfo into a
     * property map keyed by canonical field names (defined by
     * SchemaResourceKeys).
     *
     * Includes metadata such as subUri, schemaName, schemaVersion, schemaClass,
     * operations count, container name, and placeholders for extensions or
     * actions.
     *
     * @return A schema resource map suitable for D-Bus publication.
     */
    SchemaResourcesType buildSchemaResourcesPayload() const;

    Metadata metaData_;
    pldm::InstanceIdDb* instanceIdDb_ = nullptr;
    pldm::requester::Handler<pldm::requester::Request>* handler_ = nullptr;
    sdeventplus::Event& event_;
    pldm_tid_t tid_;
    /** @brief  Redfish Resource PDR list blob **/
    std::vector<std::vector<uint8_t>> pdrPayloads_;
    DeviceState currentState_;
    std::unique_ptr<ResourceRegistry> resourceRegistry_;
    std::unique_ptr<pldm::rde::DictionaryManager> dictionaryManager_;
    std::unique_ptr<DiscoverySession> discovSession_;
};

} // namespace pldm::rde
