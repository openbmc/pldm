#include "utils.hpp"

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

    for (auto it = parents.begin(); it != parents.end();)
    {
        bool find = false;
        for (const auto& evs : entityAssoc)
        {
            for (size_t i = 1; i < evs.size(); i++)
            {
                if (evs[i].entity_type == it->entity_type &&
                    evs[i].entity_instance_num == it->entity_instance_num &&
                    evs[i].entity_container_id == it->entity_container_id)
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

void addObjectPathEntityAssociations(const EntityAssociations& entityAssoc,
                                     const pldm_entity& entity,
                                     const ObjectPath& path,
                                     ObjectPathMaps& objPathMap)
{
    bool find = false;
    if (!entityMaps.contains(entity.entity_type))
    {
        std::cerr
            << "No found entity type from the std::map of entityMaps...\n";
        return;
    }

    std::string entityName = entityMaps.at(entity.entity_type);
    std::string entityNum = std::to_string(
        entity.entity_instance_num > 0 ? entity.entity_instance_num - 1 : 0);

    for (auto& ev : entityAssoc)
    {
        if (ev[0].entity_instance_num == entity.entity_instance_num &&
            ev[0].entity_type == entity.entity_type)
        {
            ObjectPath p = path / fs::path{entityName + entityNum};

            for (size_t i = 1; i < ev.size(); i++)
            {
                addObjectPathEntityAssociations(entityAssoc, ev[i], p,
                                                objPathMap);
            }
            find = true;
        }
    }

    if (!find)
    {
        objPathMap.emplace(path / fs::path{entityName + entityNum}, entity);
    }
}

void updateEntityAssociation(const EntityAssociations& entityAssoc,
                             pldm_entity_association_tree* entityTree,
                             ObjectPathMaps& objPathMap)
{
    std::vector<pldm_entity> parentsEntity = getParentEntites(entityAssoc);
    for (auto& entity : parentsEntity)
    {
        fs::path path{"/xyz/openbmc_project/inventory"};
        std::vector<std::string> paths{};
        auto node = pldm_entity_association_tree_find(entityTree, &entity);
        if (!node)
        {
            continue;
        }

        while (node)
        {
            auto pNode = pldm_entity_get_parent(node);
            if (!pNode)
            {
                break;
            }

            pldm_entity parent = pldm_entity_extract(pNode);
            try
            {
                paths.push_back(
                    entityMaps.at(parent.entity_type) +
                    std::to_string(parent.entity_instance_num > 0
                                       ? parent.entity_instance_num - 1
                                       : 0));
                node = pldm_entity_association_tree_find(entityTree, &parent);
            }
            catch (const std::exception& e)
            {
                std::cerr << "No found entity type, type = "
                          << parent.entity_type << '\n';
                break;
            }
        }

        size_t i = paths.size();
        while (i--)
        {
            path = path / fs::path{paths[i]};
        }

        addObjectPathEntityAssociations(entityAssoc, entity, path, objPathMap);
    }
}
} // namespace utils
} // namespace hostbmc
} // namespace pldm
