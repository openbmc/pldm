#include "state_set_health_state.hpp"

#include <libpldm/state_set.h>

#include <phosphor-logging/lg2.hpp>

#include <utility>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

void StateSetHealthState::setPresentState(uint8_t presentState)
{
    /* Functional is a boolean and a false is reported as a Redfish Health of
     * `Critical`, so a state which reports a condition short of critical
     * leaves the entity functional.
     */
    bool functional = false;
    switch (presentState)
    {
        case PLDM_STATE_SET_HEALTH_STATE_NORMAL:
        case PLDM_STATE_SET_HEALTH_STATE_NON_CRITICAL:
        case PLDM_STATE_SET_HEALTH_STATE_UPPER_NON_CRITICAL:
        case PLDM_STATE_SET_HEALTH_STATE_LOWER_NON_CRITICAL:
            functional = true;
            break;
        case PLDM_STATE_SET_HEALTH_STATE_CRITICAL:
        case PLDM_STATE_SET_HEALTH_STATE_UPPER_CRITICAL:
        case PLDM_STATE_SET_HEALTH_STATE_LOWER_CRITICAL:
        case PLDM_STATE_SET_HEALTH_STATE_FATAL:
        case PLDM_STATE_SET_HEALTH_STATE_UPPER_FATAL:
        case PLDM_STATE_SET_HEALTH_STATE_LOWER_FATAL:
            break;
        default:
            /* A terminus which reports a state the state set does not define
             * keeps reporting it, so each such state is logged once and a
             * repeat of the same one is not.
             */
            if (std::exchange(lastUnknownState, presentState) != presentState)
            {
                lg2::error(
                    "The health state set has no state value {STATE}, so the entity on {PATH} is not functional.",
                    "STATE", presentState, "PATH", path);
            }
            break;
    }

    interface.functional(functional);
}

} // namespace platform_mc
} // namespace pldm
