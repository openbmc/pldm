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

void StateEffecter::updatePresentState(uint8_t offset, uint8_t opState,
                                       uint8_t presentState)
{
    if (offset >= componentStateSets.size())
    {
        lg2::error(
            "Terminus ID {TID}: State effecter {EFFECTERID} has no effecter offset {OFFSET}.",
            "TID", tid, "EFFECTERID", info->pdr.effecter_id, "OFFSET", offset);
        return;
    }

    auto stateSet = componentStateSets[offset];
    if (!stateSet)
    {
        return;
    }

    if (opState != EFFECTER_OPER_STATE_ENABLED_UPDATEPENDING &&
        opState != EFFECTER_OPER_STATE_ENABLED_NOUPDATEPENDING)
    {
        return;
    }

    if (!info->componentInfo[offset].second.contains(presentState))
    {
        lg2::error(
            "Terminus ID {TID}: Effecter offset {OFFSET} of state effecter {EFFECTERID} reports state {STATE}, which is not one of its possible states.",
            "TID", tid, "OFFSET", offset, "EFFECTERID", info->pdr.effecter_id,
            "STATE", presentState);
        return;
    }

    stateSet->setPresentState(presentState);
}

} // namespace platform_mc
} // namespace pldm
