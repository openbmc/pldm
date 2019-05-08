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

TEST(GetPDR, testEncodeResponse)
{
    uint8_t completionCode = 0;
    uint32_t nextRecordHndl = 0x12;
    uint32_t nextDataTransferHndl = 0x13;
    uint8_t transferFlag = PLDM_START_AND_END;
    uint16_t respCnt = 0x5;
    std::array<uint8_t, 5> recordData{1, 2, 3, 4, 5};
    uint8_t transferCRC = 0;

    // + size of record data and transfer CRC
    std::array<uint8_t, PLDM_GET_PDR_MIN_RESP_BYTES + 5 + 1> responseMsg{};
    pldm_msg response{};

    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();

    auto rc = encode_get_pdr_resp(0, PLDM_SUCCESS, nextRecordHndl,
                                  nextDataTransferHndl, transferFlag, respCnt,
                                  recordData.data(), transferCRC, &response);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completionCode, response.body.payload[0]);

    ASSERT_EQ(0,
              memcmp(response.body.payload + sizeof(response.body.payload[0]),
                     &nextRecordHndl, sizeof(nextRecordHndl)));

    ASSERT_EQ(0,
              memcmp(response.body.payload + sizeof(response.body.payload[0]) +
                         sizeof(nextRecordHndl),
                     &nextDataTransferHndl, sizeof(nextDataTransferHndl)));

    ASSERT_EQ(0,
              memcmp(response.body.payload + sizeof(response.body.payload[0]) +
                         sizeof(nextRecordHndl) + sizeof(nextDataTransferHndl),
                     &transferFlag, sizeof(transferFlag)));

    ASSERT_EQ(0,
              memcmp(response.body.payload + sizeof(response.body.payload[0]) +
                         sizeof(nextRecordHndl) + sizeof(nextDataTransferHndl) +
                         sizeof(transferFlag),
                     &respCnt, sizeof(respCnt)));

    ASSERT_EQ(0,
              memcmp(response.body.payload + sizeof(response.body.payload[0]) +
                         sizeof(nextRecordHndl) + sizeof(nextDataTransferHndl) +
                         sizeof(transferFlag) + sizeof(respCnt),
                     recordData.data(), recordData.size()));

    ASSERT_EQ(0,
              memcmp(response.body.payload + sizeof(response.body.payload[0]) +
                         sizeof(nextRecordHndl) + sizeof(nextDataTransferHndl) +
                         sizeof(transferFlag) + sizeof(respCnt) + respCnt,
                     &transferCRC, sizeof(transferCRC)));
}

TEST(GetPDR, testGoodDecodeRequest)
{
    std::array<uint8_t, PLDM_GET_PDR_REQ_BYTES> requestMsg{};

    pldm_msg_payload request{};
    request.payload = requestMsg.data();
    request.payload_length = requestMsg.size();

    uint32_t recordHndl = 0x32;
    uint32_t dataTransferHndl = 0x11;
    uint8_t transferOpFlag = PLDM_GET_FIRSTPART;
    uint16_t requestCnt = 0x5;
    uint16_t recordChangeNum = 0;

    uint32_t retRecordHndl = 0;
    uint32_t retDataTransferHndl = 0;
    uint8_t retTransferOpFlag = 0;
    uint16_t retRequestCnt = 0;
    uint16_t retRecordChangeNum = 0;

    memcpy(request.payload, &recordHndl, sizeof(recordHndl));
    memcpy(request.payload + sizeof(recordHndl), &dataTransferHndl,
           sizeof(dataTransferHndl));
    memcpy(request.payload + sizeof(recordHndl) + sizeof(dataTransferHndl),
           &transferOpFlag, sizeof(transferOpFlag));
    memcpy(request.payload + sizeof(recordHndl) + sizeof(dataTransferHndl) +
               sizeof(transferOpFlag),
           &requestCnt, sizeof(requestCnt));
    memcpy(request.payload + sizeof(recordHndl) + sizeof(dataTransferHndl) +
               sizeof(transferOpFlag) + sizeof(requestCnt),
           &recordChangeNum, sizeof(recordChangeNum));

    auto rc = decode_get_pdr_req(&request, &retRecordHndl, &retDataTransferHndl,
                                 &retTransferOpFlag, &retRequestCnt,
                                 &retRecordChangeNum);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(retRecordHndl, recordHndl);
    ASSERT_EQ(retDataTransferHndl, dataTransferHndl);
    ASSERT_EQ(retTransferOpFlag, transferOpFlag);
    ASSERT_EQ(retRequestCnt, requestCnt);
    ASSERT_EQ(retRecordChangeNum, recordChangeNum);
}

TEST(GetPDR, testBadDecodeRequest)
{
    std::array<uint8_t, PLDM_GET_PDR_REQ_BYTES> requestMsg{};

    pldm_msg_payload request{};
    request.payload = requestMsg.data();
    request.payload_length = requestMsg.size();

    auto rc = decode_get_pdr_req(&request, NULL, NULL, NULL, NULL, NULL);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}
