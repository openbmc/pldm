#include "config.h"

#include "softoff.hpp"

#include "utils.hpp"

#include <array>
#include <iostream>
#include <sdbusplus/bus.hpp>

#include "libpldm/platform.h"
#include "libpldm/requester/pldm.h"

namespace pldm
{
using sdbusplus::exception::SdBusError;
constexpr auto TID = 1;

PldmSoftPowerOff::PldmSoftPowerOff()
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
            std::cerr << "PLDM host soft off: Host state is not Running.\n";
            hasError = true;
            return;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "PLDM host soft off: Can't get current host state.\n";
        hasError = true;
        return;
    }

    auto rc = getEffecterID();
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

    // Get Timeout seconds
    if (TIME_OUT_SECONDS > 0 && TIME_OUT_SECONDS <= 32767)
    {
        timeOutSeconds = TIME_OUT_SECONDS;
    }
}

int PldmSoftPowerOff::getEffecterID()
{
    auto bus = sdbusplus::bus::new_default();
    int PHYP_PDR_NOT_EXIST = false;

    try
    {
        std::vector<std::vector<uint8_t>> PHYP_Response{};
        auto PHYP_method = bus.new_method_call(
            "xyz.openbmc_project.PLDM.PDR", "/xyz/openbmc_project/pldm",
            "xyz.openbmc_project.PLDM.PDR", "FindStateEffecterPDR");
        PHYP_method.append(TID, PHYP_EntityType, PHYP_State_Set_ID);

        auto PHYP_ResponseMsg = bus.call(PHYP_method);

        PHYP_ResponseMsg.read(PHYP_Response);
        pldm_state_effecter_pdr* PHYP_pdr =
            reinterpret_cast<pldm_state_effecter_pdr*>(PHYP_Response.data());
        effecterID = PHYP_pdr->effecter_id;
    }
    catch (const SdBusError& e)
    {
        std::cerr << "PLDM soft off: Error get PYHP PDR,ERROR=" << e.what()
                  << "\n";
        PHYP_PDR_NOT_EXIST = true;
    }

    if (PHYP_PDR_NOT_EXIST)
    {
        try
        {
            std::vector<std::vector<uint8_t>> HostBoot_Response{};
            auto HostBoot_method = bus.new_method_call(
                "xyz.openbmc_project.PLDM.PDR", "/xyz/openbmc_project/pldm",
                "xyz.openbmc_project.PLDM.PDR", "FindStateEffecterPDR");
            HostBoot_method.append(TID, HostBoot_EntityType,
                                   HostBoot_State_Set_ID);

            auto HostBoot_ResponseMsg = bus.call(HostBoot_method);

            HostBoot_ResponseMsg.read(HostBoot_Response);
            pldm_state_effecter_pdr* HostBoot_pdr =
                reinterpret_cast<pldm_state_effecter_pdr*>(
                    HostBoot_Response.data());
            effecterID = HostBoot_pdr->effecter_id;
        }
        catch (const SdBusError& e)
        {
            std::cerr << "PLDM soft off: Error get HostBoot PDR,ERROR="
                      << e.what() << "\n";
            return PLDM_ERROR;
        }
    }

    return PLDM_SUCCESS;
}

pldm_requester_rc_t PldmSoftPowerOff::soft_off_pldm_send_recv(
    mctp_eid_t eid, int mctp_fd, const uint8_t* pldm_req_msg,
    size_t req_msg_len, uint8_t** pldm_resp_msg, size_t* resp_msg_len)
{
    struct pldm_msg_hdr* hdr = (struct pldm_msg_hdr*)pldm_req_msg;
    if ((hdr->request != PLDM_REQUEST) &&
        (hdr->request != PLDM_ASYNC_REQUEST_NOTIFY))
    {
        return PLDM_REQUESTER_NOT_REQ_MSG;
    }

    pldm_requester_rc_t rc = pldm_send(eid, mctp_fd, pldm_req_msg, req_msg_len);
    if (rc != PLDM_REQUESTER_SUCCESS)
    {
        return rc;
    }

    int count = 0;
    while (1)
    {
        rc = pldm_recv(eid, mctp_fd, hdr->instance_id, pldm_resp_msg,
                       resp_msg_len);
        if (rc == PLDM_REQUESTER_SUCCESS)
        {
            break;
        }
        if (count >= 30)
        {
            std::cerr << "Failed to receive response, Timeout!"
                      << "\n";
            break;
        }
        count++;
        sleep(1);
    }

    return rc;
}

int PldmSoftPowerOff::setStateEffecterStates()
{
    uint8_t effecterCount = 1;
    mctpEID = pldm::utils::readHostEID();

    std::array<uint8_t, sizeof(pldm_msg_hdr) + sizeof(effecterID) +
                            sizeof(effecterCount) +
                            sizeof(set_effecter_state_field)>
        requestMsg{};
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    set_effecter_state_field stateField{PLDM_REQUEST_SET, state};
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

    uint8_t* responseMsg = nullptr;
    size_t responseMsgSize{};
    // Send PLDM request msg and wait for response
    rc = soft_off_pldm_send_recv(mctpEID, fd, requestMsg.data(),
                                 requestMsg.size(), &responseMsg,
                                 &responseMsgSize);
    if (rc < 0)
    {
        std::cerr << "Failed to send message/receive response. RC = " << rc
                  << ", errno = " << errno << "\n";
        return PLDM_ERROR;
    }

    pldm_msg* response = reinterpret_cast<pldm_msg*>(responseMsg);
    std::cout << "Done. PLDM RC = " << std::hex << std::showbase
              << static_cast<uint16_t>(response->payload[0]) << std::endl;
    free(responseMsg);

    return PLDM_SUCCESS;
}
} // namespace pldm
