#include "state_set.hpp"

#include "state_set_health_state.hpp"

#include <libpldm/state_set.h>

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

std::unique_ptr<StateSetBase> createStateSet(StateSets& stateSets,
                                             StateSetId stateSetId)
{
    switch (stateSetId)
    {
        case PLDM_STATE_SET_HEALTH_STATE:
            return std::make_unique<StateSetHealthState>(stateSets);
        default:
            return nullptr;
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
        stateSet = createStateSet(*this, stateSetId);
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
