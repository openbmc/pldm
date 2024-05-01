#include "common/utils.hpp"

#include "libpldm/entity.h"

#include "utils.hpp"

#include <iostream>

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
        lg2::info(
            "{ENTITY_TYPE} Entity fetched from remote PLDM terminal does not exist.",
            "ENTITY_TYPE", (int)node_entity.entity_type);
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

            fs::path p = path / fs::path{entityName +
                                         std::to_string(
                                             node_entity.entity_instance_num)};
            std::string entity_path = p.string();
            if (oemPlatformHandler != nullptr)
            {
                oemPlatformHandler->updateOemDbusPaths(entity_path);
            }
            // If the entity obtained from the remote PLDM terminal is not in
            // the MAP, or there is no auxiliary name PDR, add it directly.
            // Otherwise, check whether the DBus service of entity_path exists,
            // and overwrite the entity if it does not exist.
            if (!objPathMap.contains(entity_path))
            {
                objPathMap[entity_path] = node_entity;
            }
            else
            {
                try
                {
                    pldm::utils::DBusHandler().getService(entity_path.c_str(),
                                                          nullptr);
                }
                catch (const std::exception& e)
                {
                    objPathMap[entity_path] = node_entity;
                }
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
        if (oemPlatformHandler != nullptr)
        {
            oemPlatformHandler->updateOemDbusPaths(dbusPath);
        }
        try
        {
            pldm::utils::DBusHandler().getService(dbusPath.c_str(), nullptr);
        }
        catch (const std::exception& e)
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

void parsingEntityMap(EntityMaps& entityMaps)
{
    const Json emptyJson{};
    std::string jsonPath{ENTITY_MAP_JSON};
    std::ifstream jsonFile(jsonPath);
    auto data = Json::parse(jsonFile);
    if (data.is_discarded())
    {
        error("Failed parsing of EntityMap data from json file: '{JSON_PATH}'",
              "JSON_PATH", jsonPath);
        return;
    }
    auto entities = data.value("EntityKeyValueMap", emptyJson);
    for (const auto& element : entities.items())
    {
        try
        {
            std::string key = static_cast<EntityName>(element.key());
            entityMaps.emplace(atoi(key.c_str()),
                               static_cast<EntityName>(element.value()));
        }
        catch (const std::exception& e)
        {
            error("Failed to create Entitymap keyvalue pair: {ERROR}", "ERROR",
                  e);
        }
    }
}

void setCoreCount(const EntityAssociations& Associations, EntityMaps entityMaps)
{
    static constexpr auto searchpath = "/xyz/openbmc_project/";
    int depth = 0;
    std::vector<std::string> cpuInterface = {
        "xyz.openbmc_project.Inventory.Item.Cpu"};
    pldm::utils::GetSubTreeResponse response =
        pldm::utils::DBusHandler().getSubtree(searchpath, depth, cpuInterface);

    // get the CPU pldm entities
    for (const auto& entries : Associations)
    {
        auto parent = pldm_entity_extract(entries[0]);
        // entries[0] would be the parent in the entity association map
        if (parent.entity_type == PLDM_ENTITY_PROC)
        {
            int corecount = 0;
            for (const auto& entry : entries)
            {
                auto child = pldm_entity_extract(entry);
                if (child.entity_type == (PLDM_ENTITY_PROC | 0x8000))
                {
                    // got a core child
                    ++corecount;
                }
            }

            auto grand_parent = pldm_entity_get_parent(entries[0]);
            std::string grepWord =
                entityMaps.at(grand_parent.entity_type) +
                std::to_string(grand_parent.entity_instance_num) + "/" +
                entityMaps.at(parent.entity_type) +
                std::to_string(parent.entity_instance_num);
            for (const auto& [objectPath, serviceMap] : response)
            {
                // find the object path with first occurance of coreX
                if (objectPath.find(grepWord) != std::string::npos)
                {
                    pldm::utils::DBusMapping dbusMapping{
                        objectPath, cpuInterface[0], "CoreCount", "uint16_t"};
                    pldm::utils::PropertyValue value =
                        static_cast<uint16_t>(corecount);
                    try
                    {
                        pldm::utils::DBusHandler().setDbusProperty(dbusMapping,
                                                                   value);
                    }
                    catch (const std::exception& e)
                    {
                        error(
                            "failed to set the core count property ERROR={ERR_EXCEP}",
                            "ERR_EXCEP", e.what());
                    }
                }
            }
        }
    }
}
} // namespace utils
} // namespace hostbmc
} // namespace pldm
