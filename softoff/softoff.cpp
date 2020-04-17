#include "config.h"

#include "softoff.hpp"

#include "libpldm/entity.h"
#include "libpldm/platform.h"
#include "libpldm/requester/pldm.h"
#include "libpldm/state_set.h"

#include "common/utils.hpp"

#include <sdbusplus/bus.hpp>
#include <sdeventplus/clock.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/source/io.hpp>
#include <sdeventplus/source/time.hpp>

#include <array>
#include <iostream>

namespace pldm
{

constexpr auto clockId = sdeventplus::ClockId::RealTime;
using Clock = sdeventplus::Clock<clockId>;
using Time = sdeventplus::source::Time<clockId>;
using namespace sdeventplus;
using namespace sdeventplus::source;

using sdbusplus::exception::SdBusError;
constexpr auto TID = 0; // TID will be implemented later.

SoftPowerOff::SoftPowerOff()
{
    auto rc = getHostState();
    if (rc == -1)
    {
        std::cerr << "Can't get the current state of host.\n";
        hasError = true;
        return;
    }
    else if (rc == 1)
    { // Host state is not "Running". Set the complete flag to
      // true and exit the pldm-softpoweroff app .
        completed = true;
        return;
    }

    rc = getEffecterID();
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Message get PDRs error. PLDM "
                     "error code = "
                  << std::hex << std::showbase << rc << "\n";
        hasError = true;
        return;
    }
    rc = setStateEffecterStates();
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Message setStateEffecterStates to host failure. PLDM "
                     "error code = "
                  << std::hex << std::showbase << rc << "\n";
        hasError = true;
        return;
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
            return 1;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "PLDM host soft off: Can't get current host state.\n";
        return -1;
    }

    return 0;
}

int SoftPowerOff::getEffecterID()
{
    auto bus = sdbusplus::bus::new_default();

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

int SoftPowerOff::setStateEffecterStates()
{
    uint8_t effecterCount = 1;
    mctpEID = pldm::utils::readHostEID();

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
        std::cerr << "Failed to init mctp"
                  << "\n";
        return PLDM_ERROR;
    }

    // Create event loop and add a callback to handle EPOLLIN on fd
    auto event = sdeventplus::Event::get_default();

    auto recvTimerFunc = [=](Time& /*source*/, Time::TimePoint /*time*/) {
        std::cerr
            << "Can't get the response for the PLDM request msg. Time out!\n";
        event.exit(PLDM_ERROR);
        return;
    };
    // Add a timer to the event loop, default 30s.
    Time time(event, (Clock(event).now() + std::chrono::seconds{30}),
              std::chrono::seconds{1}, std::move(recvTimerFunc));

    auto callback = [=](IO& io, int fd, uint32_t revents) {
        if (!(revents & EPOLLIN))
        {
            return;
        }

        uint8_t* responseMsg = nullptr;
        size_t responseMsgSize{};
        auto rc = pldm_recv(mctpEID, fd, request->hdr.instance_id, &responseMsg,
                            &responseMsgSize);
        if (!rc)
        {
            // We've got the response meant for the PLDM request msg that was
            // sent out
            io.set_enabled(Enabled::Off);
            pldm_msg* response = reinterpret_cast<pldm_msg*>(responseMsg);
            std::cerr << "Getting the response. PLDM RC = " << std::hex
                      << std::showbase
                      << static_cast<uint16_t>(response->payload[0]) << "\n";
            free(responseMsg);
            event.exit(PLDM_SUCCESS);
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

    return event.loop();
}
} // namespace pldm
