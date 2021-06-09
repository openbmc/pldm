#pragma once

#include "libpldm/entity.h"
#include "libpldm/pdr.h"

#include <deque>
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
    {PLDM_ENTITY_SYSTEM_CHASSIS, "chassis"},
    {PLDM_ENTITY_BOARD, "io_board"},
    {PLDM_ENTITY_SYS_BOARD, "motherboard"},
    {PLDM_ENTITY_POWER_SUPPLY, "powersupply"},
    {PLDM_ENTITY_MEMORY_MODULE, "dimm"},
    {PLDM_ENTITY_PROC, "cpu"},
    {PLDM_ENTITY_SYSTEM_LOGICAL, "system"}};

namespace hostbmc
{
namespace utils
{

/** @brief Vector a entity name to pldm_entity from entity association tree
 *  @param[in]  entityAssoc    - Vector of associated pldm entities
 *  @param[in]  entityTree     - entity association tree
 *  @param[out] objPathMap     - maps an object path to pldm_entity from the
 *                               BMC's entity association tree
 *  @return
 */
void updateEntityAssociation(const EntityAssociations& entityAssoc,
                             pldm_entity_association_tree* entityTree,
                             ObjectPathMaps& objPathMap);

} // namespace utils
} // namespace hostbmc
} // namespace pldm
