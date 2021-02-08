
#include "host_lamp_test.hpp"

#include "entity.h"
#include "platform.h"
#include "pldm.h"
#include "state_set.h"

#include "common/types.hpp"
#include "common/utils.hpp"

#include <iostream>

namespace pldm
{
namespace led
{

/** @brief Overloaded Property Setter function */
bool HostLampTest::asserted(bool value)
{
    uint16_t effecterID = getEffecterID();
    if (effecterID == 0)
    {
        return value;
    }

    setHostStateEffecter(effecterID);

    // Set the base class's asserted to 'true' since the getter
    // operation is handled there.
    return sdbusplus::xyz::openbmc_project::Led::server::Group::asserted(value);
}

uint16_t HostLampTest::getEffecterID()
{
    uint16_t effecterID = 0;

    // INDICATOR is a logical entity, so the bit 15 in entity type is set.
    pdr::EntityType entityType = PLDM_ENTITY_INDICATOR | 0x8000;

    auto stateEffecterPDRs = pldm::utils::findStateEffecterPDR(
        mctp_eid, entityType, (uint16_t)PLDM_STATE_SET_IDENTIFY_STATE, pdrRepo);
    for (auto& rep : stateEffecterPDRs)
    {
        auto pdr = reinterpret_cast<pldm_state_effecter_pdr*>(rep.data());
        effecterID = pdr->effecter_id;
        break;
    }

    return effecterID;
}

int HostLampTest::setHostStateEffecter(uint16_t effecterID)
{
    constexpr uint8_t effecterCount = 1;
    auto instanceId = requester.getInstanceId(mctp_eid);

    std::array<uint8_t, sizeof(pldm_msg_hdr) + sizeof(effecterID) +
                            sizeof(effecterCount) +
                            sizeof(set_effecter_state_field)>
        requestMsg{};
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    set_effecter_state_field stateField{PLDM_REQUEST_SET,
                                        PLDM_STATE_SET_IDENTIFY_STATE_ASSERTED};
    auto rc = encode_set_state_effecter_states_req(
        instanceId, effecterID, effecterCount, &stateField, request);
    if (rc != PLDM_SUCCESS)
    {
        requester.markFree(mctp_eid, instanceId);
        std::cerr << "Failed to encode_set_state_effecter_states_req, rc = "
                  << rc << std::endl;
        return PLDM_ERROR;
    }

    uint8_t* responseMsg = nullptr;
    size_t responseMsgSize{};
    auto requesterRc =
        pldm_send_recv(mctp_eid, mctp_fd, requestMsg.data(), requestMsg.size(),
                       &responseMsg, &responseMsgSize);
    std::unique_ptr<uint8_t, decltype(std::free)*> responseMsgPtr{responseMsg,
                                                                  std::free};
    requester.markFree(mctp_eid, instanceId);
    if (requesterRc)
    {
        std::cerr << "Failed to send msg to report state effecter states, rc = "
                  << requesterRc << std::endl;
        return PLDM_ERROR;
    }

    uint8_t completionCode{};
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsgPtr.get());
    rc = decode_set_state_effecter_states_resp(
        responsePtr, responseMsgSize - sizeof(pldm_msg_hdr), &completionCode);

    if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
    {
        std::cerr << "Failed to decode_set_state_effecter_states_resp: "
                  << "rc=" << rc
                  << ", cc=" << static_cast<unsigned>(completionCode)
                  << std::endl;
    }

    return rc;
}

} // namespace led
} // namespace pldm
