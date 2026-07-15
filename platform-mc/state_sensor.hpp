#pragma once

#include "common/types.hpp"

#include <libpldm/platform.h>

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
    /** @brief Fixed portion of the State Sensor PDR, up to and including
     *         compositeSensorCount
     */
    pldm_platform_state_sensor_pdr pdr;

    /** @brief State set ID and supported state values of each composite
     *         sensor offset
     */
    std::vector<std::pair<StateSetId, std::set<uint8_t>>> compositeInfo;
};

} // namespace platform_mc
} // namespace pldm
