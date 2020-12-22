#include "utils.hpp"

#include "common/utils.hpp"

#include <iostream>

namespace fs = std::filesystem;
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
         it = find ? parents.erase(it) : std::next(it))
    {
        uint16_t parent_host_contained_id =
            pldm_entity_node_get_remote_container_id(*it);
        found = false;
        for (const auto& evs : entityAssoc)
        {
            for (size_t i = 1; i < evs.size() && !found; i++)
            {
                uint16_t node_host_contained_id =
                    pldm_entity_node_get_remote_container_id(evs[i]);

                pldm_entity parent_entity = pldm_entity_extract(*it);
                pldm_entity node_entity = pldm_entity_extract(evs[i]);

                if (node_entity.entity_type == parent_entity.entity_type &&
                    node_entity.entity_instance_num ==
                        parent_entity.entity_instance_num &&
                    node_host_contained_id == parent_host_contained_id)
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

void addObjectPathEntityAssociations(const EntityAssociations& entityAssoc,
                                     pldm_entity_node* entity,
                                     const ObjectPath& path,
                                     ObjectPathMaps& objPathMap)
{
    if (entity == nullptr)
    {
        return;
    }

    bool found = false;
    pldm_entity node_entity = pldm_entity_extract(entity);
    if (!entityMaps.contains(node_entity.entity_type))
    {
        return;
    }

    std::string entityName = entityMaps.at(node_entity.entity_type);
    for (auto& ev : entityAssoc)
    {
        pldm_entity ev_entity = pldm_entity_extract(ev[0]);
        if (ev_entity.entity_instance_num == node_entity.entity_instance_num &&
            ev_entity.entity_type == node_entity.entity_type)
        {
            uint16_t node_host_contained_id =
                pldm_entity_node_get_remote_container_id(ev[0]);
            uint16_t entity_host_contained_id =
                pldm_entity_node_get_remote_container_id(entity);

            if (node_host_contained_id != entity_host_contained_id)
            {
                continue;
            }

            ObjectPath p =
                path /
                fs::path{entityName +
                         std::to_string(node_entity.entity_instance_num)};
            std::string entity_path = p.string();
            try
            {
                pldm::utils::DBusHandler().getService(entity_path.c_str(),
                                                      nullptr);
            }
            catch (const std::exception& e)
            {
                objPathMap[entity_path] = entity;
            }

            for (size_t i = 1; i < ev.size(); i++)
            {
                addObjectPathEntityAssociations(entityAssoc, ev[i], p,
                                                objPathMap);
            }
            found = true;
        }
    }

    if (!found)
    {
        std::string dbusPath =
            path / fs::path{entityName +
                            std::to_string(node_entity.entity_instance_num)};

        try
        {
            pldm::utils::DBusHandler().getService(dbusPath.c_str(), nullptr);
        }
        catch (const std::exception& e)
        {
            objPathMap[dbusPath] = entity;
        }
    }
}

void updateEntityAssociation(const EntityAssociations& entityAssoc,
                             pldm_entity_association_tree* entityTree,
                             ObjectPathMaps& objPathMap)
{
    std::vector<pldm_entity_node*> parentsEntity =
        getParentEntites(entityAssoc);
    for (auto& entity : parentsEntity)
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
                std::cerr
                    << "Parent entity not found in the entityMaps, type = "
                    << parent.entity_type
                    << ", num = " << parent.entity_instance_num
                    << ", e = " << e.what() << std::endl;
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

        addObjectPathEntityAssociations(entityAssoc, entity, path, objPathMap);
    }
}
} // namespace utils
} // namespace hostbmc
} // namespace pldm
