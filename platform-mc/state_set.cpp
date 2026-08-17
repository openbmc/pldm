#include "state_set.hpp"

#include "state_set_link_state.hpp"

#include <libpldm/state_set.h>

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

std::unique_ptr<StateSetBase> createStateSet(
    const std::string& path, const std::shared_ptr<PortIntf>& portIntf,
    StateSetId stateSetId)
{
    switch (stateSetId)
    {
        case PLDM_STATE_SET_LINK_STATE:
            /* An entity whose type does not implement
             * Inventory.Connector.Port has nowhere to publish the link
             * status.
             */
            if (!portIntf)
            {
                return nullptr;
            }
            return std::make_unique<StateSetLinkState>(path, portIntf);
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
        stateSet = createStateSet(path, portIntf, stateSetId);
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
