#include "utils.hpp"

#include <libpldm/entity.h>

#include <cstdlib>
#include <iostream>

using namespace pldm::utils;
using ordered_json = nlohmann::ordered_json;

namespace pldm
{
namespace hostbmc
{
namespace utils
{
Entities getParentEntites(const EntityAssociations& entityAssoc)
{
    Entities parents{};
    for (const auto& et : entityAssoc)
    {
        parents.push_back(et[0]);
    }

    bool found = false;
    for (auto it = parents.begin(); it != parents.end();
         it = found ? parents.erase(it) : std::next(it))
    {
        uint16_t parent_contained_id =
            pldm_entity_node_get_remote_container_id(*it);
        found = false;
        for (const auto& evs : entityAssoc)
        {
            for (size_t i = 1; i < evs.size() && !found; i++)
            {
                uint16_t node_contained_id =
                    pldm_entity_node_get_remote_container_id(evs[i]);

                pldm_entity parent_entity = pldm_entity_extract(*it);
                pldm_entity node_entity = pldm_entity_extract(evs[i]);

                if (node_entity.entity_type == parent_entity.entity_type &&
                    node_entity.entity_instance_num ==
                        parent_entity.entity_instance_num &&
                    node_contained_id == parent_contained_id)
                {
                    found = true;
                }
            }
            if (found)
            {
                break;
            }
        }
    }

    return parents;
}

void addObjectPathEntityAssociations(
    const EntityAssociations& entityAssoc, pldm_entity_node* entity,
    const fs::path& path, ObjectPathMaps& objPathMap, EntityMaps entityMaps,
    pldm::responder::oem_platform::Handler* oemPlatformHandler)
{
    if (entity == nullptr)
    {
        return;
    }

    bool found = false;
    pldm_entity node_entity = pldm_entity_extract(entity);
    if (!entityMaps.contains(node_entity.entity_type))
    {
        // entityMaps doesn't contain entity type which are not required to
        // build entity object path, so returning from here because this is a
        // expected behaviour
        return;
    }

    std::string entityName = entityMaps.at(node_entity.entity_type);
    for (const auto& ev : entityAssoc)
    {
        pldm_entity ev_entity = pldm_entity_extract(ev[0]);
        if (ev_entity.entity_instance_num == node_entity.entity_instance_num &&
            ev_entity.entity_type == node_entity.entity_type)
        {
            uint16_t node_contained_id =
                pldm_entity_node_get_remote_container_id(ev[0]);
            uint16_t entity_contained_id =
                pldm_entity_node_get_remote_container_id(entity);

            if (node_contained_id != entity_contained_id)
            {
                continue;
            }

            fs::path p =
                path /
                fs::path{entityName +
                         std::to_string(node_entity.entity_instance_num)};
            std::string entity_path = p.string();
            if (oemPlatformHandler)
            {
                oemPlatformHandler->updateOemDbusPaths(entity_path);
            }
            try
            {
                pldm::utils::DBusHandler().getService(entity_path.c_str(),
                                                      nullptr);
                // If the entity obtained from the remote PLDM terminal is not
                // in the MAP, or there is no auxiliary name PDR, add it
                // directly. Otherwise, check whether the DBus service of
                // entity_path exists, and overwrite the entity if it does not
                // exist.
                if (objPathMap.contains(entity_path))
                {
                    objPathMap[entity_path] = node_entity;
                }
            }
            catch (const std::exception&)
            {
                objPathMap[entity_path] = node_entity;
            }

            for (size_t i = 1; i < ev.size(); i++)
            {
                addObjectPathEntityAssociations(entityAssoc, ev[i], p,
                                                objPathMap, entityMaps,
                                                oemPlatformHandler);
            }
            found = true;
        }
    }

    if (!found)
    {
        std::string dbusPath =
            path / fs::path{entityName +
                            std::to_string(node_entity.entity_instance_num)};
        if (oemPlatformHandler)
        {
            oemPlatformHandler->updateOemDbusPaths(dbusPath);
        }
        try
        {
            pldm::utils::DBusHandler().getService(dbusPath.c_str(), nullptr);
            if (objPathMap.contains(dbusPath))
            {
                objPathMap[dbusPath] = node_entity;
            }
        }
        catch (const std::exception&)
        {
            objPathMap[dbusPath] = node_entity;
        }
    }
}

void updateEntityAssociation(
    const EntityAssociations& entityAssoc,
    pldm_entity_association_tree* entityTree, ObjectPathMaps& objPathMap,
    EntityMaps entityMaps,
    pldm::responder::oem_platform::Handler* oemPlatformHandler)
{
    std::vector<pldm_entity_node*> parentsEntity =
        getParentEntites(entityAssoc);
    for (const auto& entity : parentsEntity)
    {
        fs::path path{"/xyz/openbmc_project/inventory"};
        std::deque<std::string> paths{};
        pldm_entity node_entity = pldm_entity_extract(entity);
        auto node = pldm_entity_association_tree_find_with_locality(
            entityTree, &node_entity, false);
        if (!node)
        {
            continue;
        }

        bool found = true;
        while (node)
        {
            if (!pldm_entity_is_exist_parent(node))
            {
                break;
            }

            pldm_entity parent = pldm_entity_get_parent(node);
            try
            {
                paths.push_back(entityMaps.at(parent.entity_type) +
                                std::to_string(parent.entity_instance_num));
            }
            catch (const std::exception& e)
            {
                lg2::error(
                    "Parent entity not found in the entityMaps, type: {ENTITY_TYPE}, num: {NUM}, e: {ERROR}",
                    "ENTITY_TYPE", (int)parent.entity_type, "NUM",
                    (int)parent.entity_instance_num, "ERROR", e);
                found = false;
                break;
            }

            node = pldm_entity_association_tree_find_with_locality(
                entityTree, &parent, false);
        }

        if (!found)
        {
            continue;
        }

        while (!paths.empty())
        {
            path = path / fs::path{paths.back()};
            paths.pop_back();
        }

        addObjectPathEntityAssociations(entityAssoc, entity, path, objPathMap,
                                        entityMaps, oemPlatformHandler);
    }
}

EntityMaps parseEntityMap(const fs::path& filePath)
{
    const Json emptyJson{};
    EntityMaps entityMaps{};
    std::ifstream jsonFile(filePath);
    auto data = Json::parse(jsonFile);
    if (data.is_discarded())
    {
        error("Failed parsing of EntityMap data from json file: '{JSON_PATH}'",
              "JSON_PATH", filePath);
        return entityMaps;
    }
    auto entities = data.value("EntityTypeToDbusStringMap", emptyJson);
    char* err;
    try
    {
        std::ranges::transform(
            entities.items(), std::inserter(entityMaps, entityMaps.begin()),
            [&err](const auto& element) {
                std::string key = static_cast<EntityName>(element.key());
                return std::make_pair(strtol(key.c_str(), &err, 10),
                                      static_cast<EntityName>(element.value()));
            });
    }
    catch (const std::exception& e)
    {
        error(
            "Failed to create entity to DBus string mapping {ERROR} and Conversion failure is '{ERR}'",
            "ERROR", e, "ERR", err);
    }

    return entityMaps;
}

json ToJSON(const pldm::utils::SavedObjs& savedObjs)
{
    ordered_json savedObjsJson;
    for (const auto& [key1, map1] : savedObjs)
    {
        json objMapjson;
        for (const auto& [key2, tupleValue] : map1)
        {
            auto& [val1, val2, interfaceMap] = tupleValue;
            json innerMap;
            for (const auto& [key3, propertyMap] : interfaceMap)
            {
                json innerMap2;
                for (const auto& [path, property] : propertyMap)
                {
                    std::visit([&innerMap2,
                                &path](auto& arg) { innerMap2[path] = arg; },
                               property);
                }
                innerMap[key3] = innerMap2;
            }
            objMapjson[key2] = {val1, val2, innerMap};
        }
        savedObjsJson[std::to_string(key1)] = objMapjson;
    }
    return savedObjsJson;
}

pldm::utils::SavedObjs FromJSON(const json& jsonData)
{
    // std::unordered_map<uint16_t, std::unordered_map<std::string,
    // std::tuple<uint16_t, uint16_t, InterfaceMap>> outerObject;
    pldm::utils::SavedObjs savedObjs;
    for (const auto& [key1, objMap] : jsonData.items())
    {
        pldm::utils::InsideMap map1;
        for (const auto& [key2, tupleVal] : objMap.items())
        {
            uint16_t val1 = tupleVal[0].get<uint16_t>();
            uint16_t val2 = tupleVal[1].get<uint16_t>();
            pldm::utils::InterfaceMap interfaceMap;
            for (const auto& [key3, innerMap] : tupleVal[2].items())
            {
                pldm::utils::PropertyMap propertyMap;
                for (const auto& [key4, property] : innerMap.items())
                {
                    pldm::utils::PropertyValue prop;
                    if (property.is_boolean())
                    {
                        prop = property.get<bool>();
                    }
                    else if (property.is_number_integer())
                    {
                        prop = property.get<uint16_t>();
                    }
                    else if (property.is_number_float())
                    {
                        prop = property.get<double>();
                    }
                    else if (property.is_string())
                    {
                        prop = property.get<std::string>();
                    }
                    else if (property.is_array())
                    {
                        if (property[0].is_number_integer())
                        {
                            std::vector<uint8_t> vec;
                            for (const auto& elem : property)
                            {
                                vec.push_back(elem.get<uint8_t>());
                            }
                            prop = std::move(vec);
                        }
                        else if (property[0].is_string())
                        {
                            std::vector<std::string> vec;
                            for (const auto& elem : property)
                            {
                                vec.push_back(elem.get<std::string>());
                            }
                            prop = std::move(vec);
                        }
                    }
                    propertyMap[key4] = prop;
                }
                interfaceMap[key3] = propertyMap;
            }
            map1[key2] = std::make_tuple(val1, val2, interfaceMap);
        }
        uint16_t key = std::stoi(key1);
        savedObjs[key] = map1;
    }

    return savedObjs;
}

} // namespace utils
} // namespace hostbmc
} // namespace pldm
