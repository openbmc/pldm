#include "oem_event_manager.hpp"

#include "requester/handler.hpp"
#include "requester/request.hpp"

#include <config.h>
#include <libpldm/pldm.h>
#include <libpldm/utils.h>
#include <systemd/sd-journal.h>

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>

namespace pldm
{
namespace oem_meta
{

int OemEventManager::processOemMsgPollEvent(
    pldm_tid_t /* tid */, uint16_t /* eventId */,
    const uint8_t* /* eventData */, size_t /* eventDataSize */)
{
    return PLDM_SUCCESS;
}

} // namespace oem_meta
} // namespace pldm
