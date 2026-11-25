#include "state_effecter.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

StateEffecter::StateEffecter(pldm_tid_t tid,
                             std::shared_ptr<StateEffecterInfo> info,
                             std::shared_ptr<StateSets> stateSets) :
    tid(tid), info(std::move(info)), stateSets(std::move(stateSets))
{
    for (const auto& componentInfo : this->info->componentInfo)
    {
        const auto stateSetId = componentInfo.first;
        StateSetBase* stateSet = nullptr;

        if (!isWritableStateSet(stateSetId))
        {
            lg2::info(
                "Terminus ID {TID}: Skip state set {STATESETID} of state effecter {EFFECTERID} - no writable D-Bus property.",
                "TID", tid, "STATESETID", stateSetId, "EFFECTERID",
                this->info->pdr.effecter_id);
        }
        else if (this->stateSets->isEffecterStateSet(stateSetId))
        {
            lg2::error(
                "Terminus ID {TID}: Skip state set {STATESETID} of state effecter {EFFECTERID} - another state effecter controls it on {PATH}.",
                "TID", tid, "STATESETID", stateSetId, "EFFECTERID",
                this->info->pdr.effecter_id, "PATH",
                this->stateSets->getPath());
        }
        else
        {
            stateSet = this->stateSets->getStateSet(stateSetId);
            if (stateSet)
            {
                this->stateSets->addEffecterStateSet(stateSetId);
            }
        }

        componentStateSets.emplace_back(stateSet);
    }
}

bool StateEffecter::hasExposedOffset() const
{
    return std::ranges::any_of(
        componentStateSets,
        [](const StateSetBase* stateSet) { return stateSet != nullptr; });
}

} // namespace platform_mc
} // namespace pldm
