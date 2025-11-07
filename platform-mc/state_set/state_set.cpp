#include "state_set.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

std::unique_ptr<StateSet> StateSetFactory::create(
    uint16_t stateSetId, uint8_t /* sensorOffset */,
    const std::string& objectPath, const Associations& /* associations */)
{
    // State set implementations will be added here
    // Example:
    // case PLDM_STATE_SET_HEALTH_STATE:
    //     return std::make_unique<StateSetHealthState>(sensorOffset,
    //                                                   objectPath,
    //                                                   associations);

    lg2::warning(
        "Unsupported state set ID {ID} for {PATH}, no implementation available",
        "ID", stateSetId, "PATH", objectPath);

    // Returns nullptr - no implementation for this state set
    return nullptr;
}

} // namespace platform_mc
} // namespace pldm
