#pragma once

#include "common/utils.hpp"

#include <string>
#include <utility>
#include <vector>

namespace pldm
{

namespace host_associations
{

struct entity
{
    uint16_t entity_type;
    uint16_t entity_instance_num;
    uint16_t entity_container_id;

    bool operator==(const entity& e) const
    {
        return ((entity_type == e.entity_type) &&
                (entity_instance_num == e.entity_instance_num) &&
                (entity_container_id == e.entity_container_id));
    }

    bool operator<(const entity& e) const
    {
        return ((entity_type < e.entity_type) ||
                ((entity_type == e.entity_type) &&
                 (entity_instance_num < e.entity_instance_num)) ||
                ((entity_type == e.entity_type) &&
                 (entity_instance_num == e.entity_instance_num) &&
                 (entity_container_id < e.entity_container_id)));
    }
};

/** @class HostAssociationsParser
 *
 *  @brief This class parses the Host Effecter json file and monitors for the
 *         D-Bus changes for the effecters. Upon change, calls the corresponding
 *         setStateEffecterStates on the host
 */
class HostAssociationsParser
{
  public:
    HostAssociationsParser() = delete;
    HostAssociationsParser(const HostAssociationsParser&) = delete;
    HostAssociationsParser& operator=(const HostAssociationsParser&) = delete;
    HostAssociationsParser(HostAssociationsParser&&) = delete;
    HostAssociationsParser& operator=(HostAssociationsParser&&) = delete;
    virtual ~HostAssociationsParser() = default;

    /** @brief Constructor to create a HostAssociationsParser object.
     *  @param[in] jsonPath - path for the json file
     */
    HostAssociationsParser(const std::string& jsonPath)
    {
        try
        {
            parseHostAssociations(jsonPath);
        }
        catch (const std::exception& e)
        {
            std::cerr << "The json file does not exist or malformed, ERROR="
                      << e.what() << "\n";
        }
    }

    /* @brief Parses the host effecter json
     *
     * @param[in] jsonPath - path for the json file
     */
    void parseHostAssociations(const std::string& jsonPath);

    std::map<entity, std::vector<std::tuple<entity, std::string, std::string>>>
        associationsInfoMap;
};

} // namespace host_associations
} // namespace pldm
