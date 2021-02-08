
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

bool HostLampTest::asserted() const
{
    return sdbusplus::xyz::openbmc_project::Led::server::Group::asserted();
}

bool HostLampTest::asserted(bool value)
{
    // When setting the asserted property to true, need to notify the PHYP to
    // start lamp test.
    // When setting the asserted property to true again, need to notify the PHYP
    // and PHYP will restart the lamp test.
    // When setting the asserted property to false, just update the asserted
    // property, do not need to notify the PHYP to stop lamp test, PHYP will
    // wait for timeout to stop.
    if (!value &&
        value ==
            sdbusplus::xyz::openbmc_project::Led::server::Group::asserted())
    {
        return value;
    }

    if (value)
    {
        if (effecterID == 0)
        {
            effecterID = getEffecterID();
        }

        if (effecterID != 0)
        {
            // Call setHostStateEffecter method to notify PHYP to start lamp
            // test. If user set Asserted to true again, need to notify PHYP to
            // start lamptest again.
            uint8_t rc = 0;
            setHostStateEffecter(effecterID, rc);
            if (rc)
            {
                throw sdbusplus::exception::SdBusError(
                    static_cast<int>(std::errc::invalid_argument),
                    "Set Host State Effector to start lamp test failed");
            }
            else
            {
                return sdbusplus::xyz::openbmc_project::Led::server::Group::
                    asserted(value);
            }
        }
        else
        {
            throw sdbusplus::exception::SdBusError(
                static_cast<int>(std::errc::invalid_argument),
                "Get Effecter ID failed, effecter = 0");
        }
    }

    return value;
}

uint16_t HostLampTest::getEffecterID()
{
    uint16_t effecterID = 0;

    if (!pdrRepo)
    {
        return effecterID;
    }

    // INDICATOR is a logical entity, so the bit 15 in entity type is set.
    pdr::EntityType entityType = PLDM_ENTITY_INDICATOR | 0x8000;

    auto stateEffecterPDRs = pldm::utils::findStateEffecterPDR(
        mctp_eid, entityType,
        static_cast<uint16_t>(PLDM_STATE_SET_IDENTIFY_STATE), pdrRepo);

    if (stateEffecterPDRs.empty())
    {
        std::cerr
            << "Lamp Test: The state set PDR can not be found, entityType = "
            << entityType << std::endl;
        return effecterID;
    }

    for (auto& rep : stateEffecterPDRs)
    {
        auto pdr = reinterpret_cast<pldm_state_effecter_pdr*>(rep.data());
        effecterID = pdr->effecter_id;
        break;
    }

    return effecterID;
}

void HostLampTest::setHostStateEffecter(uint16_t effecterID, uint8_t& rc)
{
    constexpr uint8_t effecterCount = 1;
    auto instanceId = requester.getInstanceId(mctp_eid);

    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) + sizeof(effecterID) +
                                    sizeof(effecterCount) +
                                    sizeof(set_effecter_state_field));
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    set_effecter_state_field stateField{PLDM_REQUEST_SET,
                                        PLDM_STATE_SET_IDENTIFY_STATE_ASSERTED};
    rc = encode_set_state_effecter_states_req(
        instanceId, effecterID, effecterCount, &stateField, request);
    if (rc != PLDM_SUCCESS)
    {
        requester.markFree(mctp_eid, instanceId);
        std::cerr << "Failed to encode_set_state_effecter_states_req, rc = "
                  << rc << std::endl;
        return;
    }

    auto setStateEffecterStatesResponseHandler = [&rc](mctp_eid_t /*eid*/,
                                                       const pldm_msg* response,
                                                       size_t respMsgLen) {
        if (response == nullptr || !respMsgLen)
        {
            std::cerr << "Failed to receive response for the Set State "
                         "Effecter States\n";
            return;
        }

        uint8_t completionCode{};
        auto responsePtr = reinterpret_cast<const struct pldm_msg*>(response);
        rc = decode_set_state_effecter_states_resp(
            responsePtr, respMsgLen - sizeof(pldm_msg_hdr), &completionCode);

        if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
        {
            std::cerr << "Failed to decode_set_state_effecter_states_resp: "
                      << "rc=" << rc
                      << ", cc=" << static_cast<unsigned>(completionCode)
                      << std::endl;
        }
    };

    rc = handler.registerRequest(
        mctp_eid, instanceId, PLDM_PLATFORM, PLDM_SET_STATE_EFFECTER_STATES,
        std::move(requestMsg),
        std::move(setStateEffecterStatesResponseHandler));
    if (rc != PLDM_SUCCESS)
    {
        std::cerr
            << "Failed to send the the Set State Effecter States request\n";
    }

    return;
}

} // namespace led
} // namespace pldm
