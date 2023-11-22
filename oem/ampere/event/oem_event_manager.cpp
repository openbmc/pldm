#include "oem_event_manager.hpp"

#include "libpldm/utils.h"

#include "com/ampere/CrashCapture/Trigger/server.hpp"
#include "cper.hpp"
#include "utils.hpp"

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

void OemEventManager::pausePolling()
{
    for (auto it = timeStamp.begin(); it != timeStamp.end(); ++it)
    {
        manager->updateAvailableState(it->first, false);
    }
}

int OemEventManager::processOemMsgPollEvent(
    pldm_tid_t tid, uint16_t eventId, const uint8_t* eventData,
    [[maybe_unused]] size_t eventDataSize)
{
    long pos;
    AmpereSpecData ampHdr;
    CommonEventData evtData;
    pos = 0;

    std::memcpy((void*)&evtData, (void*)&eventData[pos],
                sizeof(CommonEventData));
    pos += sizeof(CommonEventData);

    if (!std::filesystem::is_directory(CPER_TEMP_DIR))
        std::filesystem::create_directories(CPER_TEMP_DIR);
    if (!std::filesystem::is_directory(CPER_LOG_PATH))
        std::filesystem::create_directories(CPER_LOG_PATH);

    std::string cperFile =
        std::string(CPER_TEMP_DIR) + "cper.dump." + std::to_string(tid);
    std::ofstream out(cperFile.c_str(), std::ofstream::binary);
    if (!out.is_open())
    {
        error("Can not open ofstream for CPER binary file");
        return PLDM_ERROR;
    }
    decodeCperRecord(eventData, pos, &ampHdr, out);
    if (ampHdr.typeId.member.isBert)
    {
        info("BERT is triggered. Pause polling sensors/event.");
        pausePolling();
        if (system("systemctl stop ampere-sysfw-hang-handler.service"))
        {
            error("Failed to call stop hand-detection service");
        }
    }
    out.close();

    std::string prefix = "RAS_CPER_";
    std::string primaryLogId = getUniqueEntryID(prefix);
    std::string type = "CPER";
    std::string faultLogFilePath = std::string(CPER_LOG_PATH) + primaryLogId;
    std::filesystem::copy(cperFile.c_str(), faultLogFilePath.c_str());
    std::filesystem::remove(cperFile.c_str());

    addCperSELLog(tid, eventId, &ampHdr);
    addFaultLogToRedfish(primaryLogId, type);

    if (ampHdr.typeId.member.isBert)
    {
        constexpr auto rasSrv = "com.ampere.CrashCapture.Trigger";
        constexpr auto rasPath = "/com/ampere/crashcapture/trigger";
        constexpr auto rasIntf = "com.ampere.CrashCapture.Trigger";
        using namespace sdbusplus::xyz::openbmc_project::Logging::server;
        std::variant<std::string> value(
            "com.ampere.CrashCapture.Trigger.TriggerAction.Bert");
        try
        {
            auto& bus = pldm::utils::DBusHandler::getBus();
            auto method =
                bus.new_method_call(rasSrv, rasPath, dbusProperties, "Set");
            method.append(rasIntf, "TriggerActions", value);
            bus.call_noreply(method);
        }
        catch (const std::exception& e)
        {
            error("call BERT trigger error - {ERROR}", "ERROR", e);
        }
    }

    return PLDM_SUCCESS;
}

} // namespace oem
} // namespace pldm
