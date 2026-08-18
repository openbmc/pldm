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

void StateSetPresence::setPresentState(uint8_t presentState)
{
    switch (presentState)
    {
        case PLDM_STATE_SET_PRESENCE_PRESENT:
            interface->present(true);
            break;
        case PLDM_STATE_SET_PRESENCE_NOT_PRESENT:
            interface->present(false);
            break;
        default:
            /* A `Present` of false is reported as a Redfish `State` of
             * `Absent`, a claim a state the state set does not define does not
             * support, so the presence keeps the value of the last reading a
             * state value was defined for. A terminus which reports such a
             * state keeps reporting it, so the entity is logged once.
             */
            if (!std::exchange(unknownStateLogged, true))
            {
                lg2::error(
                    "The presence state set has no state value {STATE}, so the presence of the entity on {PATH} is left unchanged.",
                    "STATE", presentState, "PATH", path);
            }
            break;
    }
}

void StateSetPerformance::setPresentState(uint8_t presentState)
{
    switch (presentState)
    {
        case PLDM_STATE_SET_PERFORMANCE_NORMAL:
            interface.performance(PerformanceValue::Normal);
            break;
        case PLDM_STATE_SET_PERFORMANCE_THROTTLED:
            interface.performance(PerformanceValue::Throttled);
            break;
        case PLDM_STATE_SET_PERFORMANCE_DEGRADED:
            interface.performance(PerformanceValue::Degraded);
            break;
        default:
            /* The interface carries no value for an entity the terminus
             * reports neither normal, throttled nor degraded, so the
             * performance keeps the value of the last reading a state value
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

void StateSetLinkState::setPresentState(uint8_t presentState)
{
    switch (presentState)
    {
        case PLDM_STATE_SET_LINK_STATE_CONNECTED:
            interface.linkStatus(LinkStatusValue::Connected);
            break;
        case PLDM_STATE_SET_LINK_STATE_DISCONNECTED:
            interface.linkStatus(LinkStatusValue::Disconnected);
            break;
        default:
            /* The interface carries no value for a link the terminus reports
             * neither connected nor disconnected, so the link status keeps the
             * value of the last reading a state value was defined for. A
             * terminus which reports such a state keeps reporting it, so the
             * entity is logged once.
             */
            if (!std::exchange(unknownStateLogged, true))
            {
                lg2::error(
                    "The link state set has no state value {STATE}, so the link status of the entity on {PATH} is left unchanged.",
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
                                  itemIntf, stateSetId);
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
