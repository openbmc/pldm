#include "deserialize.hpp"

#include "custom_dbus.hpp"
#include "serialize.hpp"

#include <libpldm/pdr.h>

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace deserialize
{
namespace fs = std::filesystem;
using namespace pldm::utils;

using Json = nlohmann::json;

using callback =
    std::function<void(const std::string& path, PropertyMap values)>;

std::unordered_map<std::string, callback> dBusInterfaceHandler{};

std::pair<std::set<uint16_t>, std::set<uint16_t>>
    getEntityTypes(const fs::path& path)
{
    std::set<uint16_t> restoreTypes{};
    std::set<uint16_t> storeTypes{};

    if (!fs::exists(path) || fs::is_empty(path))
    {
        error("The file '{PATH}' does not exist or may be empty", "PATH", path);
        return std::make_pair(restoreTypes, storeTypes);
    }

    try
    {
        std::ifstream jsonFile(path);
        auto json = Json::parse(jsonFile);

        // define the default JSON as empty
        const std::set<uint16_t> empty{};
        const Json emptyjson{};
        auto restorePaths = json.value("restore", emptyjson);
        auto storePaths = json.value("store", emptyjson);
        auto restoreobjectPaths = restorePaths.value("entityTypes", empty);
        auto storeobjectPaths = storePaths.value("entityTypes", empty);

        std::ranges::transform(
            restoreobjectPaths,
            std::inserter(restoreTypes, restoreTypes.begin()),
            [](const auto& type) { return type; });
        std::ranges::transform(storeobjectPaths,
                               std::inserter(storeTypes, storeTypes.begin()),
                               [](const auto& type) { return type; });
    }
    catch (const std::exception& e)
    {
        error("Failed to parse config file '{PATH}': {ERROR}", "PATH", path,
              "ERROR", e);
    }

    return std::make_pair(restoreTypes, storeTypes);
}

void restoreDbusObj(HostPDRHandler* hostPDRHandler)
{
    if (!hostPDRHandler)
    {
        return;
    }

    auto entityTypes = getEntityTypes(DBUS_JSON_FILE);
    pldm::serialize::Serialize::getSerialize().setEntityTypes(
        entityTypes.second);

    if (!pldm::serialize::Serialize::getSerialize().deserialize())
    {
        return;
    }

    auto savedObjs = pldm::serialize::Serialize::getSerialize().getSavedObjs();

    for (auto& [type, objs] : savedObjs)
    {
        if (!entityTypes.first.contains(type))
        {
            continue;
        }

        for (auto& [path, entites] : objs)
        {
            auto& [num, id, obj] = entites;
            pldm_entity entity{type, num, id};
            hostPDRHandler->updateObjectPathMaps(path, entity);
            for (auto& [name, propertyValue] : obj)
            {
                if (!dBusInterfaceHandler.contains(name))
                {
                    error(
                        "PropertyName '{NAME}' is not in dBusInterfaceHandler",
                        "NAME", name);
                    continue;
                }
                dBusInterfaceHandler.at(name)(path, propertyValue);
            }
        }
    }
}

} // namespace deserialize
} // namespace pldm
