#include "state_set.hpp"

#include "state_set_presence.hpp"

#include <libpldm/state_set.h>

#include <common/utils.hpp>
#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

std::unique_ptr<StateSetBase> createStateSet(
    [[maybe_unused]] sdbusplus::bus_t& bus, const std::string& path,
    const std::shared_ptr<InventoryItemServer>& itemIntf, StateSetId stateSetId)
{
    switch (stateSetId)
    {
        case PLDM_STATE_SET_PRESENCE:
            return std::make_unique<StateSetPresence>(path, itemIntf);
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
