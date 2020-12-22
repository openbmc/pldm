#pragma once

#include "libpldm/pdr.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace pldm
{
using ObjectPath = fs::path;
using EntityName = std::string;
using EntityType = uint16_t;

using Entities = std::vector<pldm_entity>;
using EntityAssociations = std::vector<Entities>;
using ObjectPathMaps = std::map<ObjectPath, pldm_entity>;

const std::map<EntityType, EntityName> entityMaps = {
    {45, "chassis"},      {60, "io_board"}, {64, "motherboard"},
    {120, "powersupply"}, {66, "dimm"},     {135, "cpu"}};

namespace hostbmc
{
namespace utils
{

/** @brief Get all parent pldm entities
 *  @param[in]  entityAssoc    - Vector of associated pldm entities
 *  @return                    - Get all parent pldm entities
 */
Entities getParentEntites(const EntityAssociations& entityAssoc);

/** @brief Vector a entity name to pldm_entity from entity association tree
 *  @param[in]  entityAssoc    - Vector of associated pldm entities
 *  @param[in]  entity         -  pldm_entity
 *  @param[in]  path           - filesystem path of D-Bus
 *  @param[out] objPathMap     - maps an object path to pldm_entity from the
 *                               BMC's entity association tree
 *  @return
 */
void addObjectPathEntityAssociations(const EntityAssociations& entityAssoc,
                                     const pldm_entity& entity,
                                     const ObjectPath& path,
                                     ObjectPathMaps& objPathMap);

} // namespace utils
} // namespace hostbmc
} // namespace pldm
