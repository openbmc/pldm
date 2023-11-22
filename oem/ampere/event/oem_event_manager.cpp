#include "oem_event_manager.hpp"

#include "libcper/sections/cper-section-ampere.h"
#include "libcper/sections/cper-section-arm.h"

#include "com/ampere/CrashCapture/Trigger/server.hpp"
#include "cper.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <filesystem>

namespace pldm
{
namespace oem
{
namespace fs = std::filesystem;
using namespace std::chrono;
constexpr auto dbusProperties = "org.freedesktop.DBus.Properties";

int OemEventManager::processOemMsgPollEvent(pldm_tid_t tid, uint16_t eventId,
                                            const uint8_t* eventData,
                                            size_t eventDataSize)
{
    EFI_AMPERE_ERROR_DATA ampHdr;

    decodeCperRecord(eventData, eventDataSize, &ampHdr);

    addCperSELLog(tid, eventId, &ampHdr);

    return PLDM_SUCCESS;
}

} // namespace oem
} // namespace pldm
