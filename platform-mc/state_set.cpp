#include "state_set.hpp"

#include <common/utils.hpp>
#include <phosphor-logging/lg2.hpp>

#include <utility>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

void StateSetPerformance::setPresentState(uint8_t presentState)
{
    switch (presentState)
    {
        case PLDM_STATE_SET_PERFORMANCE_NORMAL:
            /* The throttle is cleared before its causes, so that the causes
             * are never empty while the entity is throttled.
             */
            throttle.throttled(false);
            throttle.throttleCauses({});
            performance.degraded(false);
            break;
        case PLDM_STATE_SET_PERFORMANCE_THROTTLED:
            /* The state set reports no cause of the throttling, and the
             * causes are set before the throttle so that they are never
             * empty while the entity is throttled.
             */
            throttle.throttleCauses({ThrottleReasons::Unknown});
            throttle.throttled(true);
            performance.degraded(false);
            break;
        case PLDM_STATE_SET_PERFORMANCE_DEGRADED:
            throttle.throttled(false);
            throttle.throttleCauses({});
            performance.degraded(true);
            break;
        default:
            /* The interfaces carry no value for an entity the terminus
             * reports neither normal, throttled nor degraded, so the
             * performance keeps the values of the last reading a state value
             * was defined for. A terminus which reports such a state keeps
             * reporting it, so the entity is logged once.
             */
            if (!std::exchange(unknownStateLogged, true))
            {
                lg2::error(
                    "The performance state set has no state value {STATE}, so the performance of the entity on {PATH} is left unchanged.",
                    "STATE", presentState, "PATH", path);
            }
            break;
    }
}

StateSetBase* StateSets::getStateSet(StateSetId stateSetId)
{
    auto it = stateSets.find(stateSetId);
    if (it != stateSets.end())
    {
        return it->second.get();
    }

    std::unique_ptr<StateSetBase> stateSet{};
    try
    {
        stateSet = createStateSet(pldm::utils::DBusHandler::getBus(), path,
                                  stateSetId);
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create the interface of state set {STATESETID} on {PATH} error - {ERROR}",
            "STATESETID", stateSetId, "PATH", path, "ERROR", e);
        return nullptr;
    }

    if (!stateSet)
    {
        return nullptr;
    }

    return stateSets.emplace(stateSetId, std::move(stateSet))
        .first->second.get();
}

} // namespace platform_mc
} // namespace pldm
