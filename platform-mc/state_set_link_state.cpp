#include "state_set_link_state.hpp"

#include <libpldm/state_set.h>

#include <phosphor-logging/lg2.hpp>

#include <utility>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

void StateSetLinkState::setPresentState(uint8_t presentState)
{
    switch (presentState)
    {
        case PLDM_STATE_SET_LINK_STATE_CONNECTED:
            interface->linkStatus(LinkStatusValue::Up);
            break;
        case PLDM_STATE_SET_LINK_STATE_DISCONNECTED:
            interface->linkStatus(LinkStatusValue::Down);
            break;
        default:
            /* The state set defines no value for a link the terminus reports
             * neither connected nor disconnected, so the link status keeps the
             * value of the last reading a state value was defined for. A
             * terminus which reports such a state keeps reporting it, so each
             * such state is logged once and a repeat of the same one is not.
             */
            if (std::exchange(lastUnknownState, presentState) != presentState)
            {
                lg2::error(
                    "The link state set has no state value {STATE} on {PATH}.",
                    "STATE", presentState, "PATH", path);
            }
            break;
    }
}

} // namespace platform_mc
} // namespace pldm
