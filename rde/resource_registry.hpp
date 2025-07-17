#pragma once

#include "xyz/openbmc_project/RDE/Common/common.hpp"

#include <libpldm/platform.h>

extern "C"
{
#include "libpldm/rde.h"
}

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @file ResourceRegistry.hpp
 * @brief Provides per-device resource and schema metadata management for RDE.
 */

namespace pldm::rde
{

using OperationType =
    sdbusplus::common::xyz::openbmc_project::rde::Common::OperationType;

struct ResourceInfo
{
    std::string resourceId;           // Resource ID used for dictionary lookup
    std::string uri;                  // subURI from RDE device
    pldm_rde_schema_type schemaClass; // Schema class identifier (RDE)
    std::string schemaName;           // Schema name (e.g., "Chassis")
    std::string schemaVersion;        // Schema version (e.g., "1.2.0")
    std::string propContainResourceName;
    std::vector<OperationType> operations; // Supported RDE operations
};

/**
 * @class ResourceRegistry
 * @brief Manages schema and resource metadata for a specific RDE-capable
 * device.
 */
class ResourceRegistry
{
  public:
    // Special member functions
    ResourceRegistry() = default;
    ~ResourceRegistry() = default;
    ResourceRegistry(const ResourceRegistry&) = default;
    ResourceRegistry(ResourceRegistry&&) noexcept = default;
    ResourceRegistry& operator=(const ResourceRegistry&) = default;
    ResourceRegistry& operator=(ResourceRegistry&&) noexcept = default;

    /**
     * @brief Construct a ResourceRegistry for a specific entity.
     * @param[in] eid Entity ID identifying the device.
     * @param[in] parent Pointer to parent object for hierarchical access.
     */
    explicit ResourceRegistry(uint16_t eid, void* parent = nullptr);

    /**
     * @brief Register a resource and its schema metadata.
     * @param[in] resourceId The resource ID (e.g., "Chassis_1")
     * @param[in] info The metadata associated with the resource.
     */
    void registerResource(const std::string& resourceId,
                          const ResourceInfo& info);

    /**
     * @brief Retrieve resource metadata by resource ID.
     * @param[in] resourceId The resource ID to look up.
     * @return Reference to ResourceInfo or throws std::out_of_range if not
     * found.
     */
    const ResourceInfo& getByResourceId(const std::string& resourceId) const;

    /**
     * @brief Retrieve resource metadata by schema class.
     * @param[in] schemaClass The schema class identifier.
     * @return Reference to ResourceInfo or throws std::out_of_range if not
     * found.
     */
    const ResourceInfo& getBySchemaClass(uint16_t schemaClass) const;

    /**
     * @brief Retrieve resource metadata by URI.
     * @param[in] uri The Redfish URI.
     * @return Reference to ResourceInfo or throws std::out_of_range if not
     * found.
     */
    const ResourceInfo& getByUri(const std::string& uri) const;

    /**
     * @brief Get the resource ID associated with a given URI.
     * @param[in] uri The Redfish URI.
     * @return The corresponding resource ID.
     */
    const std::string& getResourceIdFromUri(const std::string& uri) const;

    /**
     * @brief Get the URI associated with a given resource ID.
     * @param[in] resourceId The resource ID.
     * @return The corresponding URI.
     */
    const std::string& getUriFromResourceId(
        const std::string& resourceId) const;

    /**
     * @brief Retrieve all schema information for the device.
     * @param[out] Reference to vector to populate with schema descriptors.
     */
    void getDeviceSchemaInfo(
        std::vector<std::unordered_map<std::string, std::string>>& out) const;

    /**
     * @brief Clear all registered resource metadata.
     */
    void reset();

    /**
     * @brief Save the registry to a file.
     * @param path[in] File path to save the registry.
     */
    void saveToFile(const std::string& path) const;

    /**
     * @brief Load the registry from a file.
     * @param path File path to load the registry.
     */
    void loadFromFile(const std::string& path);

    /**
     * @brief Load and parse Redfish Resource PDRs from multiple payloads.
     *
     * This function processes a batch of encoded Redfish Resource PDR payloads,
     * decoding each one into a `pldm_redfish_resource_pdr` structure.
     * Successfully decoded PDRs are stored and parsed to extract resource
     * information.
     *
     * Parsed resources are registered into the internal registry for later
     * lookup. The registry is reset before loading new data.
     *
     * @param[in] payloads A list of encoded PDR byte buffers, where each entry
     * represents a single Redfish Resource PDR payload.
     *
     * @throws std::runtime_error if decoding or parsing fails for any valid
     * payload.
     */
    void loadFromResourcePDR(const std::vector<std::vector<uint8_t>>& payloads);

    /**
     * @brief Get the current map of resource metadata entries.
     *
     * Each entry is keyed by resource ID and contains schema metadata
     * such as URI, schema name, version, operations, and classification.
     *
     * @return Reference to the internal resource map.
     */
    const std::unordered_map<std::string, ResourceInfo>& getResourceMap() const
    {
        return resourceMap_;
    }

    /**
     * @brief Convert numeric resource ID to string.
     *
     * Simple wrapper around `std::to_string` to enable uniform usage.
     *
     * @param[in] id Numeric resource ID.
     * @return String representation of the resource ID.
     */
    inline std::string toRawResourceIdString(uint32_t id)
    {
        return std::to_string(id);
    }

    /**
     * @brief Convert string representation to numeric resource ID.
     *
     * Parses raw numeric strings into `uint32_t`. Assumes well-formed input.
     *
     * @param[in] idStr Resource ID as string.
     * @return Parsed numeric resource ID.
     * @throws std::invalid_argument or std::out_of_range on bad input.
     */
    inline uint32_t fromRawResourceIdString(const std::string& idStr)
    {
        return static_cast<uint32_t>(std::stoul(idStr));
    }

  private:
    /**
     * @brief Construct the full Redfish URI for a given resource.
     *
     * This function recursively builds the URI of a resource by combining the
     * base URI, sub-URIs, and parent relationships as defined in Redfish
     * resource hierarchy.
     *
     * It looks up the provided resourceId in subUriMap to append its relative
     * URI segment. If the resource has a parent in the parentMap, the parent's
     * URI is prepended as needed to form a full hierarchical path.
     *
     * @param[in] resourceId The unique identifier of the resource.
     * @param[in] subUriMap A map of resourceId to its relative URI component.
     * @param[in] parentMap A map of resourceId to its parent resourceId.
     *
     * @return A string representing the fully constructed Redfish URI.
     */
    std::string constructFullUriRecursive(
        uint16_t resourceId,
        const std::unordered_map<uint16_t, std::string>& subUriMap,
        const std::unordered_map<uint16_t, uint16_t>& parentMap);

    /** @brief Extract the schema version.
     *
     *  This function parses the 32-bit PLDM version structure (ver32_t) and
     *  extracts the version component, returning it as a string.
     *
     *  @param[in] version - The 32-bit encoded PLDM version (ver32_t).
     *
     *  @return A string representing the version number.
     */
    std::string getMajorSchemaVersion(ver32_t& version);

    /** @brief Extract the RDE resource name from a raw byte buffer.
     *
     *  @param[in] ptr - Pointer to the raw byte buffer containing the resource
     * name.
     *  @param[in] length - Length of the byte buffer.
     *
     *  @return A string representing the decoded RDE resource name.
     */

    std::string getRdeResourceName(uint8_t* ptr, size_t length);

    /** @brief Parse a list of Redfish Resource PDRs into structured resource
     *        information.This function processes a vector of shared pointers
     *        to PLDM Redfish Resource   PDR structures and extracts relevant
     *        information from each entry to populate   a list of `ResourceInfo
     *        objects.
     *
     *  @param[in] pdrList - A vector of shared pointers to PLDM Redfish
     *             Resource PDRs.
     *
     *  @return A vector of `ResourceInfo` structures containing parsed data
     *          from the PDRs.
     */
    std::vector<ResourceInfo> parseRedfishResourcePDRs(
        const std::vector<std::shared_ptr<pldm_redfish_resource_pdr>>& pdrList);

    uint16_t entityId_;   // Entity ID of the associated device
    void* parent_;        // Pointer to parent object for context
    std::unordered_map<std::string, ResourceInfo>
        resourceMap_;     // Map from resource ID to metadata
    std::unordered_map<std::string, std::string>
        uriToResourceId_; // URI to resource ID map
    std::unordered_map<std::string, std::string>
        resourceIdToUri_; // Resource ID to URI map
    std::unordered_map<uint16_t, std::string>
        classToUri_;      // Schema class to URI map
};

} // namespace pldm::rde
