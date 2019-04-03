#include <string.h>

#include <array>

#include "libpldm/base.h"
#include "libpldm/platform.h"

#include <gtest/gtest.h>

TEST(SetStateEffecterStates, testEncodeResponse)
{
    pldm_msg response{};
    uint8_t completionCode = 0;

    std::array<uint8_t, PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES>
        responseMsg{};

    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();

    auto rc = encode_set_state_effecter_states_resp(0, PLDM_SUCCESS, &response);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completionCode, response.body.payload[0]);
}

TEST(SetStateEffecterStates, testEncodeRequest)
{
    pldm_msg request{};
    uint16_t effecterId = 0x0A;
    uint8_t compEffecterCnt = 0x1;
    std::array<set_effecter_state_field, 8> stateField{};
    stateField[0] = {PLDM_REQUEST_SET, 2};
    stateField[1] = {PLDM_REQUEST_SET, 3};

    std::array<uint8_t, PLDM_SET_STATE_EFFECTER_STATES_REQ_BYTES> requestMsg{};

    request.body.payload = requestMsg.data();
    request.body.payload_length = requestMsg.size();

    auto rc = encode_set_state_effecter_states_req(
        0, effecterId, compEffecterCnt, stateField.data(), stateField.size(), &request);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(request.body.payload, &effecterId, sizeof(effecterId)));
    ASSERT_EQ(0, memcmp(request.body.payload + sizeof(effecterId), &compEffecterCnt,
                        sizeof(compEffecterCnt)));
    ASSERT_EQ(0, memcmp(request.body.payload + sizeof(effecterId) + sizeof(compEffecterCnt), 
                 &stateField, sizeof(stateField)));
}

TEST(SetStateEffecterStates, testGoodDecodeResponse)
{
    std::array<uint8_t, PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES>
        responseMsg{};

    pldm_msg_payload response{};
    response.payload = responseMsg.data();
    response.payload_length = responseMsg.size();

    uint8_t completion_code = 0xA0;

    uint8_t retcompletion_code = 0;

    memcpy(response.payload, &completion_code, sizeof(completion_code));

    auto rc =
        decode_set_state_effecter_states_resp(&response, &retcompletion_code);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completion_code, retcompletion_code);
}

TEST(SetStateEffecterStates, testGoodDecodeRequest)
{
    std::array<uint8_t, PLDM_SET_STATE_EFFECTER_STATES_REQ_BYTES> requestMsg{};

    pldm_msg_payload request{};
    request.payload = requestMsg.data();
    request.payload_length = requestMsg.size();

    uint16_t effecterId = 0x32;
    uint8_t compEffecterCnt = 0x2;

    std::array<set_effecter_state_field, 8> stateField{};
    stateField[0] = {PLDM_REQUEST_SET, 3};
    stateField[1] = {PLDM_REQUEST_SET, 4};

    uint16_t retEffecterId = 0;
    uint8_t retCompEffecterCnt = 0;

    std::array<set_effecter_state_field, 8> retStateField{};

    memcpy(request.payload, &effecterId, sizeof(effecterId));
    memcpy(request.payload + sizeof(effecterId), &compEffecterCnt,
           sizeof(compEffecterCnt));
    memcpy(request.payload + sizeof(effecterId) + sizeof(compEffecterCnt),
           &stateField, sizeof(stateField));

    auto rc = decode_set_state_effecter_states_req(
        &request, &retEffecterId, &retCompEffecterCnt, retStateField.data());

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(effecterId, retEffecterId);
    ASSERT_EQ(retCompEffecterCnt, compEffecterCnt);
    ASSERT_EQ(retStateField[0].set_request, stateField[0].set_request);
    ASSERT_EQ(retStateField[0].effecter_state, stateField[0].effecter_state);
    ASSERT_EQ(retStateField[1].set_request, stateField[1].set_request);
    ASSERT_EQ(retStateField[1].effecter_state, stateField[1].effecter_state);
}

TEST(SetStateEffecterStates, testBadDecodeRequest)
{
    std::array<uint8_t, PLDM_SET_STATE_EFFECTER_STATES_REQ_BYTES> requestMsg{};

    pldm_msg_payload request{};
    request.payload = requestMsg.data();
    request.payload_length = requestMsg.size();

    auto rc = decode_set_state_effecter_states_req(&request, NULL, NULL, NULL);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(SetStateEffecterStates, testBadDecodeResponse)
{
    std::array<uint8_t, PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES>
        responseMsg{};

    pldm_msg_payload response{};
    response.payload = responseMsg.data();
    response.payload_length = responseMsg.size();

    auto rc = decode_set_state_effecter_states_resp(&response, NULL);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}
