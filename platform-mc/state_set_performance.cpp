#include "state_set_performance.hpp"

#include <libpldm/state_set.h>

#include <phosphor-logging/lg2.hpp>

#include <utility>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

void StateSetPerformance::setThrottled(bool throttled)
{
    if (throttled)
    {
        throttle.throttleCauses({ThrottleReasons::Unknown});
        throttle.throttled(true);
    }
    else
    {
        throttle.throttled(false);
        throttle.throttleCauses({});
    }
}

void StateSetPerformance::setPresentState(uint8_t presentState)
{
    switch (presentState)
    {
        case PLDM_STATE_SET_PERFORMANCE_NORMAL:
            setThrottled(false);
            performance.degraded(false);
            break;
        case PLDM_STATE_SET_PERFORMANCE_THROTTLED:
            setThrottled(true);
            performance.degraded(false);
            break;
        case PLDM_STATE_SET_PERFORMANCE_DEGRADED:
            setThrottled(false);
            performance.degraded(true);
            break;
        default:
            /* The interfaces carry no value for an entity the terminus
             * reports neither normal, throttled nor degraded, so the
             * performance keeps the values of the last reading a state value
             * was defined for. A terminus which reports such a state keeps
             * reporting it, so each such state is logged once and a repeat of
             * the same one is not.
             */
            if (std::exchange(lastUnknownState, presentState) != presentState)
            {
                lg2::error(
                    "The performance state set has no state value {STATE}, so the performance of the entity on {PATH} is left unchanged.",
                    "STATE", presentState, "PATH", path);
            }
            break;
    }
}

} // namespace platform_mc
} // namespace pldm
