#include "utils.hpp"

#include <iostream>

namespace fs = std::filesystem;
namespace pldm
{
namespace hostbmc
{
namespace utils
{

Entities getParentEntites(EntityAssociations& entityAssoc)
{
    Entities parents{};
    for (auto& et : entityAssoc)
    {
        parents.push_back(et[0]);
    }

    for (auto it = parents.begin(); it != parents.end();)
    {
        uint16_t parent_host_contanied_id = pldm_extract_host_container_id(*it);
        bool find = false;
        for (auto& evs : entityAssoc)
        {
            for (size_t i = 1; i < evs.size(); i++)
            {
                uint16_t node_host_contanied_id =
                    pldm_extract_host_container_id(evs[i]);

                if (pldm_entity_extract(evs[i]).entity_type ==
                        pldm_entity_extract(*it).entity_type &&
                    pldm_entity_extract(evs[i]).entity_instance_num ==
                        pldm_entity_extract(*it).entity_instance_num &&
                    parent_host_contanied_id == node_host_contanied_id &&
                    node_host_contanied_id != 0xFFFF)
                {
                    find = true;
                    break;
                }
            }
            if (find)
            {
                break;
            }
        }
        if (find)
        {
            it = parents.erase(it);
        }
        else
        {
            it++;
        }
    }

    return parents;
}

void addObjectPathEntityAssociations(EntityAssociations& entityAssoc,
                                     pldm_entity_association_tree* entityTree,
                                     pldm_entity_node* entity,
                                     const ObjectPath& path,
                                     ObjectPathMaps& objPathMap)
{
    if (entity == nullptr)
    {
        return;
    }
    bool find = false;
    if (!entityMaps.contains(pldm_entity_extract(entity).entity_type))
    {
        std::cerr << "No found entity type from the std::map of entityMaps, "
                     "entity type = "
                  << pldm_entity_extract(entity).entity_type << std::endl;
        return;
    }

    std::string entityName =
        entityMaps.at(pldm_entity_extract(entity).entity_type);
    for (auto& ev : entityAssoc)
    {

        if (pldm_entity_extract(ev[0]).entity_instance_num ==
                pldm_entity_extract(entity).entity_instance_num &&
            pldm_entity_extract(ev[0]).entity_type ==
                pldm_entity_extract(entity).entity_type)
        {
            uint16_t node_host_contanied_id =
                pldm_extract_host_container_id(ev[0]);
            uint16_t entity_host_contanied_id =
                pldm_extract_host_container_id(entity);

            if (node_host_contanied_id != entity_host_contanied_id)
            {
                continue;
            }
            ObjectPath p =
                path /
                fs::path{entityName +
                         std::to_string(
                             pldm_entity_extract(entity).entity_instance_num)};

            for (size_t i = 1; i < ev.size(); i++)
            {
                addObjectPathEntityAssociations(entityAssoc, entityTree, ev[i],
                                                p, objPathMap);
            }
            find = true;
        }
    }

    if (!find)
    {
        objPathMap.emplace(
            path /
                fs::path{entityName +
                         std::to_string(
                             pldm_entity_extract(entity).entity_instance_num)},
            std::move(entity));
    }
}

void updateEntityAssociation(EntityAssociations& entityAssoc,
                             pldm_entity_association_tree* entityTree,
                             ObjectPathMaps& objPathMap)
{
    std::vector<pldm_entity_node*> parentsEntity =
        getParentEntites(entityAssoc);
    for (auto& entity : parentsEntity)
    {
        fs::path path{"/xyz/openbmc_project/inventory/system1/chassis1"};
        std::deque<std::string> paths{};
        pldm_entity e = pldm_entity_extract(entity);
        auto node = pldm_entity_association_tree_find(entityTree, &e, true);
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

            node = pldm_entity_association_tree_find(entityTree, &parent, true);
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

        addObjectPathEntityAssociations(entityAssoc, entityTree, entity, path,
                                        objPathMap);
    }
}
} // namespace utils
} // namespace hostbmc
} // namespace pldm
