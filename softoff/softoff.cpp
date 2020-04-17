#include "config.h"

#include "softoff.hpp"

#include "libpldm/entity.h"
#include "libpldm/platform.h"
#include "libpldm/requester/pldm.h"
#include "libpldm/state_set.h"

#include "common/utils.hpp"

#include <sdbusplus/bus.hpp>
#include <sdeventplus/clock.hpp>
#include <sdeventplus/source/io.hpp>
#include <sdeventplus/source/time.hpp>

#include <array>
#include <iostream>

namespace pldm
{

using namespace sdeventplus;
using namespace sdeventplus::source;
constexpr auto clockId = sdeventplus::ClockId::RealTime;
using Clock = Clock<clockId>;
using Timer = Time<clockId>;

using sdbusplus::exception::SdBusError;
constexpr uint8_t TID = 0; // TID will be implemented later.

SoftPowerOff::SoftPowerOff(sdbusplus::bus::bus& bus, sd_event* event) :
    bus(bus), timer(event)
{
    auto rc = getHostState();
    if (hasError || completed)
    {
        return;
    }

    rc = getEffecterID();
    if (rc != PLDM_SUCCESS)
    {
        hasError = true;
        return;
    }

    // Start Timer
    using namespace std::chrono;
    auto time = duration_cast<microseconds>(seconds(SOFTOFF_TIMEOUT_SECONDS));

    rc = startTimer(time);
    if (rc < 0)
    {
        std::cerr << "Failure to start Host soft off wait timer, ERRNO = " << rc
                  << "\n";
        hasError = true;
        return;
    }
    else
    {
        std::cerr
            << "Timer started waiting for host soft off, TIMEOUT_IN_SEC = "
            << SOFTOFF_TIMEOUT_SECONDS << "\n";
    }
}

int SoftPowerOff::getHostState()
{
    try
    {
        pldm::utils::PropertyValue propertyValue =
            pldm::utils::DBusHandler().getDbusPropertyVariant(
                "/xyz/openbmc_project/state/host0", "CurrentHostState",
                "xyz.openbmc_project.State.Host");

        if (std::get<std::string>(propertyValue) !=
            "xyz.openbmc_project.State.Host.HostState.Running")
        {
            // Host state is not "Running", this app should return success
            completed = true;
            return PLDM_SUCCESS;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "PLDM host soft off: Can't get current host state.\n";
        hasError = true;
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

int SoftPowerOff::getEffecterID()
{
    auto& bus = pldm::utils::DBusHandler::getBus();

    try
    {
        std::vector<std::vector<uint8_t>> VMMResponse{};
        auto VMMMethod = bus.new_method_call(
            "xyz.openbmc_project.PLDM", "/xyz/openbmc_project/pldm",
            "xyz.openbmc_project.PLDM.PDR", "FindStateEffecterPDR");
        VMMMethod.append(TID, (uint16_t)PLDM_ENTITY_VIRTUAL_MACHINE_MANAGER,
                         (uint16_t)PLDM_STATE_SET_SW_TERMINATION_STATUS);

        auto VMMResponseMsg = bus.call(VMMMethod);

        VMMResponseMsg.read(VMMResponse);
        if (VMMResponse.size() != 0)
        {
            for (auto& rep : VMMResponse)
            {
                auto VMMPdr =
                    reinterpret_cast<pldm_state_effecter_pdr*>(rep.data());
                effecterID = VMMPdr->effecter_id;
            }
        }
        else
        {
            VMMPdrExist = false;
        }
    }
    catch (const SdBusError& e)
    {
        std::cerr << "PLDM soft off: Error get VMM PDR,ERROR=" << e.what()
                  << "\n";
        VMMPdrExist = false;
    }

    if (VMMPdrExist == false)
    {
        try
        {
            std::vector<std::vector<uint8_t>> sysFwResponse{};
            auto sysFwMethod = bus.new_method_call(
                "xyz.openbmc_project.PLDM", "/xyz/openbmc_project/pldm",
                "xyz.openbmc_project.PLDM.PDR", "FindStateEffecterPDR");
            sysFwMethod.append(TID, (uint16_t)PLDM_ENTITY_SYS_FIRMWARE,
                               (uint16_t)PLDM_STATE_SET_SW_TERMINATION_STATUS);

            auto sysFwResponseMsg = bus.call(sysFwMethod);

            sysFwResponseMsg.read(sysFwResponse);
            if (sysFwResponse.size() != 0)
            {
                for (auto& rep : sysFwResponse)
                {
                    auto sysFwPdr =
                        reinterpret_cast<pldm_state_effecter_pdr*>(rep.data());
                    effecterID = sysFwPdr->effecter_id;
                }
            }
            else
            {
                std::cerr
                    << "No effecter ID has been found that matches the criteria"
                    << "\n";
                return PLDM_ERROR;
            }
        }
        catch (const SdBusError& e)
        {
            std::cerr << "PLDM soft off: Error get system firmware PDR,ERROR="
                      << e.what() << "\n";
            return PLDM_ERROR;
        }
    }

    return PLDM_SUCCESS;
}

int SoftPowerOff::hostSoftOff(sdeventplus::Event& event)
{
    constexpr uint8_t effecterCount = 1;
    uint8_t mctpEID;
    uint8_t instanceID;

    mctpEID = pldm::utils::readHostEID();

    // Get instanceID
    try
    {
        auto& bus = pldm::utils::DBusHandler::getBus();
        auto method = bus.new_method_call(
            "xyz.openbmc_project.PLDM", "/xyz/openbmc_project/pldm",
            "xyz.openbmc_project.PLDM.Requester", "GetInstanceId");
        method.append(mctpEID);

        auto ResponseMsg = bus.call(method);

        ResponseMsg.read(instanceID);
    }
    catch (const SdBusError& e)
    {
        std::cerr << "PLDM soft off: Error get instanceID,ERROR=" << e.what()
                  << "\n";
        return PLDM_ERROR;
    }

    std::array<uint8_t, sizeof(pldm_msg_hdr) + sizeof(effecterID) +
                            sizeof(effecterCount) +
                            sizeof(set_effecter_state_field)>
        requestMsg{};
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    set_effecter_state_field stateField{
        PLDM_REQUEST_SET, PLDM_SW_TERM_GRACEFUL_SHUTDOWN_REQUESTED};
    auto rc = encode_set_state_effecter_states_req(0, effecterID, effecterCount,
                                                   &stateField, request);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Message encode failure. PLDM error code = " << std::hex
                  << std::showbase << rc << "\n";
        return PLDM_ERROR;
    }

    // Open connection to MCTP socket
    int fd = pldm_open();
    if (-1 == fd)
    {
        std::cerr << "Failed to connect to mctp demux daemon"
                  << "\n";
        return PLDM_ERROR;
    }

    // Create event loop and add a callback to handle EPOLLIN on fd
    auto timerCallback = [=](Timer& /*source*/, Timer::TimePoint /*time*/) {
        if (!responseReceived)
        {
            std::cerr << "PLDM soft off: ERROR! Can't get the response for the "
                         "PLDM request msg. Time out!\n"
                      << "Exit the pldm-softpoweroff\n";
            exit(-1);
        }
        return;
    };
    // Add a timer to the event loop, default 30s.
    Timer time(event, (Clock(event).now() + std::chrono::seconds{30}),
               std::chrono::seconds{1}, std::move(timerCallback));

    auto callback = [=](IO& io, int fd, uint32_t revents) {
        if (!(revents & EPOLLIN))
        {
            return;
        }

        if (responseReceived)
        {
            return;
        }

        uint8_t* responseMsg = nullptr;
        size_t responseMsgSize{};

        auto rc = pldm_recv(mctpEID, fd, request->hdr.instance_id, &responseMsg,
                            &responseMsgSize);
        std::unique_ptr<uint8_t, decltype(std::free)*> responseMsgPtr{
            responseMsg, std::free};
        if (!rc)
        {
            // We've got the response meant for the PLDM request msg that was
            // sent out
            io.set_enabled(Enabled::Off);
            auto response = reinterpret_cast<pldm_msg*>(responseMsgPtr.get());
            std::cerr << "Getting the response. PLDM RC = " << std::hex
                      << std::showbase
                      << static_cast<uint16_t>(response->payload[0]) << "\n";
            responseReceived = true;
            return;
        }
    };
    IO io(event, fd, EPOLLIN, std::move(callback));

    // Send PLDM Request message - pldm_send doesn't wait for response
    rc = pldm_send(mctpEID, fd, requestMsg.data(), requestMsg.size());
    if (0 > rc)
    {
        std::cerr << "Failed to send message/receive response. RC = " << rc
                  << ", errno = " << errno << "\n";
        return PLDM_ERROR;
    }

    // Time out or soft off complete
    while (!isCompleted() && !isTimerExpired())
    {
        try
        {
            event.run(std::nullopt);
        }
        catch (const sdeventplus::SdEventError& e)
        {
            std::cerr
                << "PLDM host soft off: Failure in processing request.ERROR= "
                << e.what() << "\n";
            return PLDM_ERROR;
        }
    }

    return PLDM_SUCCESS;
}

int SoftPowerOff::startTimer(const std::chrono::microseconds& usec)
{
    return timer.start(usec);
}
} // namespace pldm
