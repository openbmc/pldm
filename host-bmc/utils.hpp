#pragma once

#include "libpldm/pdr.h"

#include <filesystem>
#include <map>
#include <string>

namespace fs = std::filesystem;

namespace pldm
{
using ObjectPath = fs::path;
using EntityName = std::string;
using EntityType = uint16_t;

using EntityMaps = std::map<EntityName, pldm_entity>;
using EntityAssociationMaps = std::map<EntityName, EntityMaps>;
using ObjectPathMaps = std::map<ObjectPath, pldm_entity>;

namespace hostbmc
{
namespace utils
{

/** @brief maps a entity name to pldm_entity from entity association tree
 *  @param[in]  entityAssociationMap    - maps an entity name to map, maps to
 *                                        entity name to pldm_entity
 *  @param[in]  entityMaps              - maps an entity name to pldm_entity
 *  @param[in]  path                    - filesystem path of D-Bus
 *  @param[out] objPathMap              - maps an object path to pldm_entity
 *                                        from the BMC's entity association tree
 *  @return
 */
void addObjectPathEntityAssociationMap(
    const EntityAssociationMaps& entityAssociationMap,
    const EntityMaps& entityMaps, const ObjectPath& path,
    ObjectPathMaps& objPathMap);

} // namespace utils
} // namespace hostbmc
} // namespace pldm
