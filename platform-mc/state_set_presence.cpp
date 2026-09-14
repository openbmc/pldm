#include "state_set_presence.hpp"

#include <libpldm/state_set.h>

#include <phosphor-logging/lg2.hpp>

#include <utility>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

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
            if (!std::exchange(unknownStateLogged, true))
            {
                lg2::error(
                    "The presence state set has no state value {STATE}, so the presence of the entity on {PATH} is left unchanged.",
                    "STATE", presentState, "PATH", path);
            }
            break;
    }
}

} // namespace platform_mc
} // namespace pldm
