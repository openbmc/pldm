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
    uint8_t compEffecterCnt = 0x2;
    std::array<set_effecter_state_field, 8> stateField{};
    stateField[0] = {PLDM_REQUEST_SET, 2};
    stateField[1] = {PLDM_REQUEST_SET, 3};

    std::array<uint8_t, PLDM_SET_STATE_EFFECTER_STATES_REQ_BYTES> requestMsg{};

    request.body.payload = requestMsg.data();
    request.body.payload_length = requestMsg.size();

    auto rc = encode_set_state_effecter_states_req(
        0, effecterId, compEffecterCnt, stateField.data(), &request);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(effecterId, request.body.payload[0]);
    ASSERT_EQ(compEffecterCnt, request.body.payload[sizeof(effecterId)]);
    ASSERT_EQ(
        stateField[0].set_request,
        request.body.payload[sizeof(effecterId) + sizeof(compEffecterCnt)]);
    ASSERT_EQ(
        stateField[0].effecter_state,
        request.body.payload[sizeof(effecterId) + sizeof(compEffecterCnt) +
                             sizeof(stateField[0].set_request)]);
    ASSERT_EQ(
        stateField[1].set_request,
        request.body.payload[sizeof(effecterId) + sizeof(compEffecterCnt) +
                             sizeof(stateField[0])]);
    ASSERT_EQ(
        stateField[1].effecter_state,
        request.body.payload[sizeof(effecterId) + sizeof(compEffecterCnt) +
                             sizeof(stateField[0]) +
                             sizeof(stateField[1].set_request)]);
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

TEST(GetPDR, testEncodeRequest)
{
    uint32_t record_handle = 0x32;
    uint32_t data_transfer_handle = 0x11;
    uint8_t transfer_operation_flag = PLDM_GET_FIRSTPART;
    uint16_t request_count = 0x5;
    uint16_t record_change_number = 0;

    std::array<uint8_t, PLDM_GET_PDR_REQ_BYTES> requestMsg{};
    pldm_msg request{};

    request.body.payload = requestMsg.data();
    request.body.payload_length = requestMsg.size();

    auto rc = encode_get_pdr_req(0, record_handle, data_transfer_handle,
                                 transfer_operation_flag, request_count,
                                 record_change_number, &request);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(
        0, memcmp(request.body.payload, &record_handle, sizeof(record_handle)));
    ASSERT_EQ(0, memcmp(request.body.payload + sizeof(record_handle),
                        &data_transfer_handle, sizeof(data_transfer_handle)));
    ASSERT_EQ(0, memcmp(request.body.payload + sizeof(record_handle) +
                            sizeof(data_transfer_handle),
                        &transfer_operation_flag,
                        sizeof(transfer_operation_flag)));
    ASSERT_EQ(0, memcmp(request.body.payload + sizeof(record_handle) +
                            sizeof(data_transfer_handle) +
                            sizeof(transfer_operation_flag),
                        &request_count, sizeof(request_count)));
    ASSERT_EQ(0, memcmp(request.body.payload + sizeof(record_handle) +
                            sizeof(data_transfer_handle) +
                            sizeof(transfer_operation_flag) +
                            sizeof(request_count),
                        &record_change_number, sizeof(record_change_number)));
}

TEST(GetPDR, testGoodDecodeResponse)
{
    // + size of record data and transfer CRC
    std::array<uint8_t, PLDM_GET_PDR_MIN_RESP_BYTES + 5 + 1> responseMsg{};
    pldm_msg_payload response{};
    response.payload = responseMsg.data();
    response.payload_length = responseMsg.size();

    uint8_t completion_code = 0;
    uint32_t next_record_handle = 0x12;
    uint32_t next_data_transfer_handle = 0x13;
    uint8_t transfer_flag = PLDM_START_AND_END;
    uint16_t response_count = 0x5;
    std::array<uint8_t, 5> record_data{1, 2, 3, 4, 5};
    uint8_t transfer_crc = 0;

    uint8_t retcompletion_code = 0;
    uint32_t retnext_record_handle = 0;
    uint32_t retnext_data_transfer_handle = 0;
    uint8_t rettransfer_flag = PLDM_START;
    uint16_t retresponse_count = 0;
    std::array<uint8_t, 5> retrecord_data{};
    uint8_t rettransfer_crc = 0;

    memcpy(response.payload, &completion_code, sizeof(completion_code));
    memcpy(response.payload + sizeof(completion_code), &next_record_handle,
           sizeof(next_record_handle));
    memcpy(response.payload + sizeof(completion_code) +
               sizeof(next_record_handle),
           &next_data_transfer_handle, sizeof(next_data_transfer_handle));
    memcpy(response.payload + sizeof(completion_code) +
               sizeof(next_record_handle) + sizeof(next_data_transfer_handle),
           &transfer_flag, sizeof(transfer_flag));
    memcpy(response.payload + sizeof(completion_code) +
               sizeof(next_record_handle) + sizeof(next_data_transfer_handle) +
               sizeof(transfer_flag),
           &response_count, sizeof(response_count));
    memcpy(response.payload + sizeof(completion_code) +
               sizeof(next_record_handle) + sizeof(next_data_transfer_handle) +
               sizeof(transfer_flag) + sizeof(response_count),
           record_data.data(), record_data.size());
    memcpy(response.payload + sizeof(completion_code) +
               sizeof(next_record_handle) + sizeof(next_data_transfer_handle) +
               sizeof(transfer_flag) + sizeof(response_count) +
               record_data.size(),
           &transfer_crc, sizeof(transfer_crc));

    auto rc = decode_get_pdr_resp(
        &response, &retcompletion_code, &retnext_record_handle,
        &retnext_data_transfer_handle, &rettransfer_flag, &retresponse_count,
        retrecord_data.data(), &rettransfer_crc);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(retcompletion_code, completion_code);
    ASSERT_EQ(retnext_record_handle, next_record_handle);
    ASSERT_EQ(retnext_data_transfer_handle, next_data_transfer_handle);
    ASSERT_EQ(rettransfer_flag, transfer_flag);
    ASSERT_EQ(retresponse_count, response_count);
    ASSERT_EQ(0, memcmp(retrecord_data.data(), record_data.data(),
                        record_data.size()));
    ASSERT_EQ(rettransfer_crc, transfer_crc);
}

TEST(GetPDR, testBadDecodeResponse)
{
    std::array<uint8_t, PLDM_GET_PDR_MIN_RESP_BYTES + 5 + 1> responseMsg{};
    pldm_msg_payload response{};
    response.payload = responseMsg.data();
    response.payload_length = responseMsg.size();

    auto rc = decode_get_pdr_resp(&response, NULL, NULL, NULL, NULL, NULL, NULL,
                                  NULL);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}
