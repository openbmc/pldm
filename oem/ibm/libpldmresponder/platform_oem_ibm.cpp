#include "platform_oem_ibm.hpp"

#include "libpldm/platform_oem_ibm.h"
#include "libpldm/requester/pldm.h"

#include "common/utils.hpp"
#include "libpldmresponder/pdr.hpp"

#include <iostream>

namespace pldm
{
namespace responder
{
namespace platform
{

int sendBiosAttributeUpdateEvent(int fd, uint8_t eid,
                                 dbus_api::Requester* requester,
                                 const std::vector<uint16_t>& handles)
{
    constexpr auto hostStatePath = "/xyz/openbmc_project/state/host0";
    constexpr auto hostStateInterface =
        "xyz.openbmc_project.State.Boot.Progress";
    constexpr auto hostStateProperty = "BootProgress";

    try
    {
        auto propVal = pldm::utils::DBusHandler().getDbusPropertyVariant(
            hostStatePath, hostStateProperty, hostStateInterface);
        const auto& currHostState = std::get<std::string>(propVal);
        if ((currHostState != "xyz.openbmc_project.State.Boot.Progress."
                              "ProgressStages.SystemInitComplete") &&
            (currHostState != "xyz.openbmc_project.State.Boot.Progress."
                              "ProgressStages.OSRunning") &&
            (currHostState != "xyz.openbmc_project.State.Boot.Progress."
                              "ProgressStages.OSStart"))
        {
            return PLDM_SUCCESS;
        }
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        std::cerr << "Error in getting current host state, continue ... \n";
    }

    auto instanceId = requester->getInstanceId(eid);

    std::vector<uint8_t> requestMsg(
        sizeof(pldm_msg_hdr) + sizeof(pldm_bios_attribute_update_event_req) -
            1 + (handles.size() * sizeof(uint16_t)),
        0);

    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = encode_bios_attribute_update_event_req(
        instanceId, PLDM_PLATFORM_EVENT_MESSAGE_FORMAT_VERSION,
        pldm::responder::pdr::BmcMctpEid, handles.size(),
        reinterpret_cast<const uint8_t*>(handles.data()),
        requestMsg.size() - sizeof(pldm_msg_hdr), request);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Message encode failure 1. PLDM error code = " << std::hex
                  << std::showbase << rc << "\n";
        requester->markFree(eid, instanceId);
        return rc;
    }

    if (requestMsg.size())
    {
        std::ostringstream tempStream;
        for (int byte : requestMsg)
        {
            tempStream << std::setfill('0') << std::setw(2) << std::hex << byte
                       << " ";
        }
        std::cout << tempStream.str() << std::endl;
    }

    uint8_t* responseMsg = nullptr;
    size_t responseMsgSize{};

    rc = pldm_send_recv(eid, fd, requestMsg.data(), requestMsg.size(),
                        &responseMsg, &responseMsgSize);
    std::unique_ptr<uint8_t, decltype(std::free)*> responseMsgPtr{responseMsg,
                                                                  std::free};
    requester->markFree(eid, instanceId);

    if (rc != PLDM_REQUESTER_SUCCESS)
    {
        std::cerr << "Failed to send BIOS attribute update event. RC = " << rc
                  << ", errno = " << errno << "\n";
        pldm::utils::reportError(
            "xyz.openbmc_project.bmc.pldm.InternalFailure");
        return rc;
    }

    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsgPtr.get());
    uint8_t completionCode{};
    uint8_t status{};
    rc = decode_platform_event_message_resp(
        responsePtr, responseMsgSize - sizeof(pldm_msg_hdr), &completionCode,
        &status);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Failed to decode PlatformEventMessage response, rc = "
                  << rc << "\n";
        return rc;
    }

    if (completionCode != PLDM_SUCCESS)
    {
        std::cerr << "Failed to send the BIOS attribute update event, rc = "
                  << (uint32_t)completionCode << "\n";
        pldm::utils::reportError(
            "xyz.openbmc_project.bmc.pldm.InternalFailure");
    }

    return completionCode;
}

void Watchdog::checkIsSetEventReceiverSent()
{
    if ((!isSetEventReceiverSent) && (!isHostOff))
    {
        isSetEventReceiverSent = true;
        return;
    }
    else if ((isSetEventReceiverSent) && (isHostOff))
    {
        isHostOff = false;
        return;
    }

    disableWatchDogTimer();
    return;
}

bool Watchdog::checkIfWatchDogRunning()
{
    static constexpr auto watchDogObjectPath =
        "/xyz/openbmc_project/watchdog/host0";
    static constexpr auto watchDogEnablePropName = "Enabled";
    static constexpr auto watchDogInterface =
        "xyz.openbmc_project.State.Watchdog";
    bool isWatchDogRunning = false;
    try
    {
        isWatchDogRunning = pldm::utils::DBusHandler().getDbusProperty<bool>(
            watchDogObjectPath, watchDogEnablePropName, watchDogInterface);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to check if Watchdog is running"
                  << "ERROR=" << e.what() << std::endl;
        return false;
    }
    return isWatchDogRunning;
}

void Watchdog::resetWatchDogTimer()
{
    static constexpr auto watchDogService = "xyz.openbmc_project.Watchdog";
    static constexpr auto watchDogObjectPath =
        "/xyz/openbmc_project/watchdog/host0";
    static constexpr auto watchDogInterface =
        "xyz.openbmc_project.State.Watchdog";
    static constexpr auto watchDogResetPropName = "ResetTimeRemaining";

    bool wdStatus = checkIfWatchDogRunning();
    try
    {
        if (wdStatus == true)
        {
            auto& bus = pldm::utils::DBusHandler::getBus();
            auto resetMethod =
                bus.new_method_call(watchDogService, watchDogObjectPath,
                                    watchDogInterface, watchDogResetPropName);
            resetMethod.append(true);
            bus.call_noreply(resetMethod);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed To reset watchdog timer"
                  << "ERROR=" << e.what() << std::endl;
        return;
    }
}

void Watchdog::disableWatchDogTimer()
{
    bool val = false;
    pldm::utils::PropertyValue value = static_cast<bool>(val);
    pldm::utils::DBusMapping dbusMapping{"/xyz/openbmc_project/watchdog/host0",
                                         "/xyz/openbmc_project/watchdog/host0",
                                         "Enabled", "bool"};
    bool wdStatus = checkIfWatchDogRunning();
    try
    {
        if (wdStatus == true)
        {
            pldm::utils::DBusHandler().setDbusProperty(dbusMapping, value);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed To extend watchdog timer"
                  << "ERROR=" << e.what() << std::endl;
        return;
    }
}

} // namespace platform

} // namespace responder

} // namespace pldm
