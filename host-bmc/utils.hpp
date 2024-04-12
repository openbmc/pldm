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
using Json = nlohmann::json;
namespace fs = std::filesystem;
using ObjectPath = fs::path;
using EntityName = std::string;
using EntityType = uint16_t;
using Entities = std::vector<pldm_entity_node*>;
using EntityAssociations = std::vector<Entities>;
using ObjectPathMaps = std::map<ObjectPath, pldm_entity>;
using EntityMaps = std::map<EntityType, EntityName>;
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
void updateEntityAssociation(
    const EntityAssociations& entityAssoc,
    pldm_entity_association_tree* entityTree, ObjectPathMaps& objPathMap,
    EntityMaps entityMaps,
    pldm::responder::oem_platform::Handler* oemPlatformHandler);

/** @brief parsing Entity map data from json file
 *  which can be configured as per requirement
 */
void parsingEntityMap(EntityMaps& entityMaps);

} // namespace utils
} // namespace hostbmc
} // namespace pldm
