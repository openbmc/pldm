#include "libpldmresponder/platform.hpp"

#include <string.h>

#include <array>

#include "libpldm/base.h"

#include <gtest/gtest.h>

using namespace pldm::responder;

TEST(setStateEffecterStates, testGoodRequest)
{
    pldm_msg response{};
    std::array<uint8_t, PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES>
        responseMsg{};
    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();
    pldm_msg_payload request{};
    std::array<uint8_t, PLDM_SET_STATE_EFFECTER_STATES_REQ_BYTES> requestMsg{};
    request.payload = requestMsg.data();
    request.payload_length = requestMsg.size();

    uint16_t effecterId = 0x32;
    uint8_t compEffecterCnt = 0x1;
    state_field_set_state_effecter_state stateField = {REQUEST_SET, 3};

    memcpy(request.payload, &effecterId, sizeof(effecterId));
    memcpy(request.payload + sizeof(effecterId), &compEffecterCnt,
           sizeof(compEffecterCnt));
    memcpy(request.payload + sizeof(effecterId) + sizeof(compEffecterCnt),
           &stateField, sizeof(stateField));

    setStateEffecterStates(&request, &response);

    ASSERT_EQ(response.body.payload[0], 0);
}

TEST(setStateEffecterStates, testBadRequest)
{
    pldm_msg response{};
    std::array<uint8_t, PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES>
        responseMsg{};
    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();
    pldm_msg_payload request{};
    std::array<uint8_t, (PLDM_SET_STATE_EFFECTER_STATES_REQ_BYTES - 16)>
        requestMsg{};
    request.payload = requestMsg.data();
    request.payload_length = requestMsg.size();

    setStateEffecterStates(&request, &response);

    ASSERT_EQ(response.body.payload[0], PLDM_ERROR_INVALID_LENGTH);
}
