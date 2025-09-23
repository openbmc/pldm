#include "resource_registry.hpp"

#include <libpldm/platform.h>

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace pldm::rde
{

ResourceRegistry::ResourceRegistry(const pldm::eid eid, void* parent) :
    entityId_(eid), parent_(parent)
{}

void ResourceRegistry::registerResource(const std::string& resourceId,
                                        const ResourceInfo& info)
{
    resourceMap_[resourceId] = info;
    uriToResourceId_[info.uri] = resourceId;
    resourceIdToUri_[resourceId] = info.uri;
    classToUri_[info.schemaClass] = info.uri;
}

const ResourceInfo& ResourceRegistry::getByUri(const std::string& uri) const
{
    auto it = resourceMap_.find(uri);
    if (it == resourceMap_.end())
    {
        throw std::out_of_range("URI not found in resourceMap");
    }
    return it->second;
}

const ResourceInfo& ResourceRegistry::getBySchemaClass(
    uint16_t schemaClass) const
{
    auto it = classToUri_.find(schemaClass);
    if (it == classToUri_.end())
    {
        throw std::out_of_range("Schema class not found");
    }
    return getByUri(it->second);
}

const std::string& ResourceRegistry::getResourceIdFromUri(
    const std::string& uri) const
{
    auto it = uriToResourceId_.find(uri);
    if (it == uriToResourceId_.end())
    {
        throw std::out_of_range("URI not found in uriToResourceId" + uri);
    }
    return it->second;
}

const std::string& ResourceRegistry::getUriFromResourceId(
    const std::string& resourceId) const
{
    auto it = resourceIdToUri_.find(resourceId);
    if (it == resourceIdToUri_.end())
    {
        throw std::out_of_range("Resource ID not found in resourceIdToUri");
    }
    return it->second;
}
void ResourceRegistry::getDeviceSchemaInfo(
    std::vector<std::unordered_map<std::string, std::string>>& out) const
{
    for (const auto& [resourceId, info] : resourceMap_)
    {
        for (const auto& op : info.operations)
        {
            std::string opStr;
            switch (op)
            {
                case OperationType::HEAD:
                    opStr = "HEAD";
                    break;
                case OperationType::READ:
                    opStr = "READ";
                    break;
                case OperationType::CREATE:
                    opStr = "CREATE";
                    break;
                case OperationType::DELETE:
                    opStr = "DELETE";
                    break;
                case OperationType::UPDATE:
                    opStr = "UPDATE";
                    break;
                case OperationType::REPLACE:
                    opStr = "REPLACE";
                    break;
                case OperationType::ACTION:
                    opStr = "ACTION";
                    break;
                default:
                    opStr = "UNKNOWN";
                    break;
            }

            std::unordered_map<std::string, std::string> entry;

            entry["uri"] = info.uri;
            entry["schemaName"] = info.schemaName;
            entry["schemaVersion"] = info.schemaVersion;
            entry["schemaClass"] = std::to_string(info.schemaClass);
            entry["ProposedContainingResourceName"] =
                info.propContainResourceName;
            entry["operation"] = opStr;

            out.push_back(std::move(entry));
        }
    }
}

void ResourceRegistry::reset()
{
    resourceMap_.clear();
    uriToResourceId_.clear();
    resourceIdToUri_.clear();
    classToUri_.clear();
}

void ResourceRegistry::saveToFile(const std::string& path) const
{
    nlohmann::json j;
    for (const auto& [resourceId, info] : resourceMap_)
    {
        j.push_back(
            {{"subURI", info.uri},
             {"schemaClass", info.schemaClass},
             {"schemaName", info.schemaName},
             {"schemaVersion", info.schemaVersion},
             {"ProposedContainingResourceName", info.propContainResourceName},
             {"operations", info.operations},
             {"resourceId", resourceId}});
    }

    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("Failed to open file for writing: " + path);
    }

    file << j.dump(4);
}

void ResourceRegistry::loadFromFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file)
    {
        throw std::runtime_error("Failed to open file for reading: " + path);
    }

    nlohmann::json j;
    file >> j;

    reset(); // Clear any previously registered resources

    for (const auto& entry : j)
    {
        ResourceInfo info;
        info.uri = entry.at("subURI").get<std::string>();
        info.schemaClass = entry.at("schemaClass").get<pldm_rde_schema_type>();
        info.schemaName = entry.at("schemaName").get<std::string>();
        info.schemaVersion = entry.at("schemaVersion").get<std::string>();
        info.propContainResourceName =
            entry.at("ProposedContainingResourceName").get<std::string>();
        info.operations =
            entry.at("operations").get<std::vector<OperationType>>();
        std::string resourceId = entry.at("resourceId").get<std::string>();

        registerResource(resourceId, info);
    }
}

std::string ResourceRegistry::getRdeResourceName(uint8_t* ptr, size_t length)
{
    if (ptr == nullptr || length == 0)
    {
        return {};
    }

    // Remove trailing null character if present
    if (ptr[length - 1] == '\0')
    {
        --length;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return std::string(reinterpret_cast<const char*>(ptr), length);
}

std::string ResourceRegistry::getMajorSchemaVersion(ver32_t& version)
{
    constexpr int MaxSize = 1024;
    char version_buffer[MaxSize] = {0};
    int rc = 0;

    if (version.alpha == 0xFF && version.update == 0xFF &&
        version.minor == 0xFF && version.major == 0xFF)
    {
        return std::string("?.?");
    }
    else
    {
        rc = ver2str(&version, version_buffer, sizeof(version_buffer));
        if (rc <= 0)
            return "?.?";
    }
    return std::string(version_buffer, rc);
}

std::string ResourceRegistry::constructFullUriRecursive(
    uint16_t resourceId,
    const std::unordered_map<uint16_t, std::string>& subUriMap,
    const std::unordered_map<uint16_t, uint16_t>& parentMap)
{
    if (resourceId == 0 || subUriMap.find(resourceId) == subUriMap.end())
        return "";

    std::string part = subUriMap.at(resourceId);
    uint16_t parentId =
        parentMap.count(resourceId) ? parentMap.at(resourceId) : 0;

    // Root node
    if (parentId == 0 && !part.empty())
    {
        return part;
    }

    std::string parentUri =
        constructFullUriRecursive(parentId, subUriMap, parentMap);

    if (!parentUri.empty() && parentUri.back() != '/')
        parentUri += "/";

    // TODO: Workaround for RDE PDRs that redundantly encode segments like
    // "0/0/Settings" If parent URI ends with the same segment as the beginning
    // of `part`, skip duplication
    std::vector<std::string> parentSegments;
    std::stringstream ss(parentUri);
    std::string segment;
    while (std::getline(ss, segment, '/'))
    {
        if (!segment.empty())
            parentSegments.push_back(segment);
    }

    std::vector<std::string> partSegments;
    std::stringstream ps(part);
    while (std::getline(ps, segment, '/'))
    {
        if (!segment.empty())
            partSegments.push_back(segment);
    }

    if (!parentSegments.empty() && !partSegments.empty() &&
        parentSegments.back() == partSegments.front())
    {
        // Skip the first segment of `part` to avoid duplication
        partSegments.erase(partSegments.begin());
    }

    for (const auto& seg : partSegments)
    {
        parentUri += seg + "/";
    }

    // Remove trailing slash if added
    if (!parentUri.empty() && parentUri.back() == '/')
        parentUri.pop_back();

    return parentUri;
}

std::vector<ResourceInfo> ResourceRegistry::parseRedfishResourcePDRs(
    const std::vector<std::shared_ptr<pldm_redfish_resource_pdr>>& pdrList)
{
    std::unordered_map<uint16_t, std::string> subUriMap;
    std::unordered_map<uint16_t, uint16_t> parentMap;
    std::unordered_map<uint16_t, ResourceInfo> resInfoMap;
    std::vector<ResourceInfo> resInfoVect;

    for (const auto& pdr : pdrList)
    {
        uint16_t rid = static_cast<uint16_t>(pdr->resource_id);
        uint16_t parent = static_cast<uint16_t>(pdr->cont_resrc_id);
        bool isRoot = (parent == 0);

        std::string proposedRoot = getRdeResourceName(
            pdr->prop_cont_resrc_name, pdr->prop_cont_resrc_length);
        std::string subUri =
            getRdeResourceName(pdr->sub_uri_name, pdr->sub_uri_length);

        std::string fullUri;
        if (isRoot)
        {
            if (!subUri.empty())
            {
                fullUri = subUri;
            }
        }
        else
        {
            fullUri = subUri;
        }

        subUriMap[rid] = fullUri;
        parentMap[rid] = parent;

        for (size_t i = 0; i < pdr->add_resrc_id_count; ++i)
        {
            add_resrc_t* add = pdr->additional_resrc[i];
            uint16_t addId = static_cast<uint16_t>(add->resrc_id);
            std::string addUri = getRdeResourceName(add->name, add->length);
            subUriMap[addId] = addUri;
            parentMap[addId] = rid;
        }

        ResourceInfo info;
        info.resourceId = std::to_string(rid);
        info.schemaName = getRdeResourceName(pdr->major_schema.name,
                                             pdr->major_schema.length);
        info.schemaVersion = getMajorSchemaVersion(pdr->major_schema_version);
        info.schemaClass = PLDM_RDE_SCHEMA_MAJOR;
        info.propContainResourceName = proposedRoot;
        resInfoMap[rid] = info;
    }

    for (const auto& [rid, _] : subUriMap)
    {
        ResourceInfo info =
            resInfoMap.count(rid) ? resInfoMap[rid] : ResourceInfo{};
        info.resourceId = std::to_string(rid);

        std::string uri = constructFullUriRecursive(rid, subUriMap, parentMap);

        // Remove leading '/' if present
        if (!uri.empty() && uri.front() == '/')
        {
            uri.erase(0, 1);
        }

        info.uri = uri;
        resInfoVect.push_back(info);
    }

    return resInfoVect;
}

void ResourceRegistry::loadFromResourcePDR(
    const std::vector<std::vector<uint8_t>>& payloads)
{
    // Clear registry before loading new data
    reset();

    std::vector<std::shared_ptr<pldm_redfish_resource_pdr>> pdrList;

    for (const auto& data : payloads)
    {
        if (data.empty())
        {
            continue; // Skip empty payloads
        }

        const uint8_t* ptr = data.data();
        auto parsedPdr = std::make_shared<pldm_redfish_resource_pdr>();
        auto rc =
            decode_redfish_resource_pdr_data(ptr, data.size(), parsedPdr.get());

        if (rc != 0 || !parsedPdr)
        {
            throw std::runtime_error(
                "Failed to decode one of the Redfish Resource PDRs");
        }

        pdrList.push_back(parsedPdr);
    }

    std::vector<ResourceInfo> resourceInfos;
    try
    {
        resourceInfos = parseRedfishResourcePDRs(pdrList);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error(
            std::string("Failed to parse resource info: ") + e.what());
    }

    for (const auto& info : resourceInfos)
    {
        const std::string& resourceId = info.resourceId;
        registerResource(resourceId, info);
    }

    constexpr const char* kResourceRegistryPrefix = "/tmp/ResourceRegistry_";
    std::string filename =
        std::string(kResourceRegistryPrefix) + std::to_string(entityId_);
    saveToFile(filename);
}

} // namespace pldm::rde
