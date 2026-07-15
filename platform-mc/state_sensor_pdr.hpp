#pragma once

#include "common/types.hpp"

#include <cstdint>
#include <set>
#include <utility>
#include <vector>

namespace pldm
{
namespace platform_mc
{

using namespace pldm::pdr;

/** @struct StateSensorInfo
 *
 *  The parsed State Sensor PDR: the fixed fields the framework needs plus,
 *  for each composite sensor offset, the state set ID and the possible state
 *  values parsed from the possible_states[] region. This is the parse-layer
 *  representation the state sensor object creation consumes in a later commit.
 */
struct StateSensorInfo
{
    /** @brief Fixed fields taken from the State Sensor PDR header. Held in a
     *  local struct rather than a libpldm PDR type so the framework does not
     *  depend on a specific decode routine.
     */
    struct
    {
        uint16_t sensor_id;
        uint16_t entity_type;
        uint16_t entity_instance_number;
        uint16_t container_id;
        uint8_t composite_sensor_count;
    } pdr;

    /** @brief State set ID and supported state values of each composite
     *         sensor offset
     */
    std::vector<std::pair<StateSetId, std::set<uint8_t>>> compositeInfo;
};

} // namespace platform_mc
} // namespace pldm
