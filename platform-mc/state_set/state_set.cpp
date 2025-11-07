#include "state_set.hpp"

#include "health_state.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

std::unique_ptr<StateSet> StateSetFactory::create(
    uint16_t stateSetId, uint8_t sensorOffset, const std::string& objectPath,
    const Associations& associations)
{
    std::unique_ptr<StateSet> stateSet = nullptr;

    switch (stateSetId)
    {
        case PLDM_STATE_SET_HEALTH_STATE:
            lg2::info("Creating Health State state set for {PATH}", "PATH",
                      objectPath);
            stateSet = std::make_unique<StateSetHealthState>(
                sensorOffset, objectPath, associations);
            break;

            // Future state sets from DSP0249 can be added here:
            // case PLDM_STATE_SET_AVAILABILITY:
            //     stateSet = std::make_unique<StateSetAvailability>(...);
            //     break;
            // case PLDM_STATE_SET_OPERATIONAL_STATUS:
            //     stateSet = std::make_unique<StateSetOperationalStatus>(...);
            //     break;

        default:
            lg2::warning(
                "Unsupported state set ID {ID} for {PATH}, creating generic state set",
                "ID", stateSetId, "PATH", objectPath);
            // Could return a generic state set or nullptr
            break;
    }

    return stateSet;
}

} // namespace platform_mc
} // namespace pldm
