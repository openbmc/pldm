#include "state_set.hpp"

#include <common/utils.hpp>
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
             * keeps reporting it, so the entity is logged once.
             */
            if (!std::exchange(unknownStateLogged, true))
            {
                lg2::error(
                    "The health state set has no state value {STATE}, so the entity on {PATH} is not functional.",
                    "STATE", presentState, "PATH", path);
            }
            break;
    }

    interface.functional(functional);
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
