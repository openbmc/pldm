#include "libpldm/pdr.h"

#include "libpldmresponder/oem_handler.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

PHOSPHOR_LOG2_USING;
namespace pldm
{
using EntityName = std::string;
using EntityType = uint16_t;

using Entities = std::vector<pldm_entity_node*>;
using EntityAssociations = std::vector<Entities>;
using ObjectPathMaps = std::map<fs::path, pldm_entity_node*>;
namespace hostbmc
{
namespace utils
{

const std::map<EntityType, EntityName> entityMaps = {
    {PLDM_ENTITY_SYSTEM_CHASSIS, "chassis"},
    {PLDM_ENTITY_BOARD, "io_board"},
    {PLDM_ENTITY_SYS_BOARD, "motherboard"},
    {PLDM_ENTITY_POWER_SUPPLY, "powersupply"},
    {PLDM_ENTITY_PROC, "cpu"},
    {PLDM_ENTITY_SYSTEM_CHASSIS | 0x8000, "system"},
    {PLDM_ENTITY_PROC_MODULE, "dcm"},
    {PLDM_ENTITY_PROC | 0x8000, "core"},
    {PLDM_ENTITY_IO_MODULE, "io_module"},
    {PLDM_ENTITY_FAN, "fan"},
    {PLDM_ENTITY_SYS_MGMT_MODULE, "system_management_module"},
    {PLDM_ENTITY_POWER_CONVERTER, "power_converter"},
    {PLDM_ENTITY_SLOT, "slot"},
    {PLDM_ENTITY_CONNECTOR, "connector"}};

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
