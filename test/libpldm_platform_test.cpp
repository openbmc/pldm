#include <string.h>

#include <array>

#include "libpldm/base.h"
#include "libpldm/platform.h"

#include <gtest/gtest.h>

TEST(SetStateEffecterStates, testEncodeResponse)
{
    std::array<uint8_t,
               sizeof(pldm_msg_hdr) + PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES>
        responseMsg{};
    pldm_msg* response = reinterpret_cast<pldm_msg*>(responseMsg.data());
    uint8_t completionCode = 0;

    auto rc = encode_set_state_effecter_states_resp(0, PLDM_SUCCESS, response);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completionCode, response->payload[0]);
}

TEST(SetStateEffecterStates, testEncodeRequest)
{
    uint16_t effecterId = 0x0A;
    uint8_t compEffecterCnt = 0x1;
    set_effecter_state_field stateField = {PLDM_REQUEST_SET, 2};

    std::array<uint8_t, PLDM_SET_STATE_EFFECTER_STATES_REQ_BYTES> requestMsg{};
    pldm_msg* request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = encode_set_state_effecter_states_req(
        0, effecterId, compEffecterCnt, &stateField, request);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(request->payload, &effecterId, sizeof(effecterId)));
    ASSERT_EQ(0, memcmp(request->payload + sizeof(effecterId), &compEffecterCnt,
                        sizeof(compEffecterCnt)));
    ASSERT_EQ(0, memcmp(request->payload + sizeof(effecterId) +
                            sizeof(compEffecterCnt),
                        &stateField, sizeof(stateField)));
}

TEST(SetStateEffecterStates, testGoodDecodeResponse)
{
    std::array<uint8_t, PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES>
        responseMsg{};

    uint8_t completion_code = 0xA0;

    uint8_t retcompletion_code = 0;

    memcpy(responseMsg.data(), &completion_code, sizeof(completion_code));

    auto rc = decode_set_state_effecter_states_resp(responseMsg.data(),
                                                    &retcompletion_code);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completion_code, retcompletion_code);
}

TEST(SetStateEffecterStates, testGoodDecodeRequest)
{
    std::array<uint8_t, PLDM_SET_STATE_EFFECTER_STATES_REQ_BYTES> requestMsg{};

    uint16_t effecterId = 0x32;
    uint8_t compEffecterCnt = 0x2;

    std::array<set_effecter_state_field, 8> stateField{};
    stateField[0] = {PLDM_REQUEST_SET, 3};
    stateField[1] = {PLDM_REQUEST_SET, 4};

    uint16_t retEffecterId = 0;
    uint8_t retCompEffecterCnt = 0;

    std::array<set_effecter_state_field, 8> retStateField{};

    memcpy(requestMsg.data(), &effecterId, sizeof(effecterId));
    memcpy(requestMsg.data() + sizeof(effecterId), &compEffecterCnt,
           sizeof(compEffecterCnt));
    memcpy(requestMsg.data() + sizeof(effecterId) + sizeof(compEffecterCnt),
           &stateField, sizeof(stateField));

    auto rc = decode_set_state_effecter_states_req(
        requestMsg.data(), &retEffecterId, &retCompEffecterCnt,
        retStateField.data());

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
    const uint8_t* msg = NULL;

    auto rc = decode_set_state_effecter_states_req(msg, NULL, NULL, NULL);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(SetStateEffecterStates, testBadDecodeResponse)
{
    std::array<uint8_t, PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES>
        responseMsg{};

    auto rc = decode_set_state_effecter_states_resp(responseMsg.data(), NULL);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}
