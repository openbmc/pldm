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
                    evs[i].entity_instance_num == it->entity_instance_num)
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
        std::cerr << "No found entity type from the std::map of entityMaps, "
                     "entity type = "
                  << entity.entity_type << std::endl;
        return;
    }

    std::string entityName = entityMaps.at(entity.entity_type);
    for (auto& ev : entityAssoc)
    {
        if (ev[0].entity_instance_num == entity.entity_instance_num &&
            ev[0].entity_type == entity.entity_type)
        {
            ObjectPath p =
                path / fs::path{entityName +
                                std::to_string(entity.entity_instance_num)};

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
        objPathMap.emplace(
            path / fs::path{entityName +
                            std::to_string(entity.entity_instance_num)},
            entity);
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
        std::deque<std::string> paths{};
        auto node = pldm_entity_association_tree_find(entityTree, &entity);
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

            node = pldm_entity_association_tree_find(entityTree, &parent);
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
