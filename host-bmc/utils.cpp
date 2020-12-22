#include "utils.hpp"

namespace fs = std::filesystem;
namespace pldm
{
namespace hostbmc
{
namespace utils
{

void addObjectPathEntityAssociationMap(
    const EntityAssociationMaps& entityAssociationMap,
    const EntityMaps& entityMaps, const ObjectPath& path,
    ObjectPathMaps& objPathMap)
{
    for (const auto& entity : entityMaps)
    {
        if (entityAssociationMap.find(entity.first) ==
            entityAssociationMap.end())
        {
            ObjectPath objPath =
                path /
                fs::path{entity.first +
                         std::to_string(entity.second.entity_instance_num)};
            objPathMap.emplace(objPath, entity.second);
        }
        else
        {
            const fs::path p =
                path /
                fs::path{entity.first +
                         std::to_string(entity.second.entity_instance_num)};
            addObjectPathEntityAssociationMap(
                entityAssociationMap, entityAssociationMap.at(entity.first), p,
                objPathMap);
        }
    }
}
} // namespace utils
} // namespace hostbmc
} // namespace pldm
