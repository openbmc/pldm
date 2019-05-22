#include <string.h>

#include <array>

#include "libpldm/base.h"
#include "libpldm/platform.h"

#include <gtest/gtest.h>

constexpr auto hdrSize = sizeof(pldm_msg_hdr);

TEST(SetStateEffecterStates, testEncodeResponse)
{
    std::array<uint8_t,
               sizeof(pldm_msg_hdr) + PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES>
        responseMsg{};
    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());
    uint8_t completionCode = 0;

    auto rc = encode_set_state_effecter_states_resp(0, PLDM_SUCCESS, response);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completionCode, response->payload[0]);
}

TEST(SetStateEffecterStates, testEncodeRequest)
{
    std::array<uint8_t,
               sizeof(pldm_msg_hdr) + PLDM_SET_STATE_EFFECTER_STATES_REQ_BYTES>
        requestMsg{};
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    uint16_t effecterId = 0x0A;
    uint8_t compEffecterCnt = 0x2;
    std::array<set_effecter_state_field, 8> stateField{};
    stateField[0] = {PLDM_REQUEST_SET, 2};
    stateField[1] = {PLDM_REQUEST_SET, 3};

    auto rc = encode_set_state_effecter_states_req(
        0, effecterId, compEffecterCnt, stateField.data(), request);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(effecterId, request->payload[0]);
    ASSERT_EQ(compEffecterCnt, request->payload[sizeof(effecterId)]);
    ASSERT_EQ(stateField[0].set_request,
              request->payload[sizeof(effecterId) + sizeof(compEffecterCnt)]);
    ASSERT_EQ(stateField[0].effecter_state,
              request->payload[sizeof(effecterId) + sizeof(compEffecterCnt) +
                               sizeof(stateField[0].set_request)]);
    ASSERT_EQ(stateField[1].set_request,
              request->payload[sizeof(effecterId) + sizeof(compEffecterCnt) +
                               sizeof(stateField[0])]);
    ASSERT_EQ(stateField[1].effecter_state,
              request->payload[sizeof(effecterId) + sizeof(compEffecterCnt) +
                               sizeof(stateField[0]) +
                               sizeof(stateField[1].set_request)]);
}

TEST(SetStateEffecterStates, testGoodDecodeResponse)
{
    std::array<uint8_t, hdrSize + PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES>
        responseMsg{};

    uint8_t completion_code = 0xA0;

    uint8_t retcompletion_code = 0;

    memcpy(responseMsg.data() + hdrSize, &completion_code,
           sizeof(completion_code));

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = decode_set_state_effecter_states_resp(
        response, responseMsg.size() - hdrSize, &retcompletion_code);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completion_code, retcompletion_code);
}

TEST(SetStateEffecterStates, testGoodDecodeRequest)
{
    std::array<uint8_t, hdrSize + PLDM_SET_STATE_EFFECTER_STATES_REQ_BYTES>
        requestMsg{};

    uint16_t effecterId = 0x32;
    uint8_t compEffecterCnt = 0x2;

    std::array<set_effecter_state_field, 8> stateField{};
    stateField[0] = {PLDM_REQUEST_SET, 3};
    stateField[1] = {PLDM_REQUEST_SET, 4};

    uint16_t retEffecterId = 0;
    uint8_t retCompEffecterCnt = 0;

    std::array<set_effecter_state_field, 8> retStateField{};

    memcpy(requestMsg.data() + hdrSize, &effecterId, sizeof(effecterId));
    memcpy(requestMsg.data() + sizeof(effecterId) + hdrSize, &compEffecterCnt,
           sizeof(compEffecterCnt));
    memcpy(requestMsg.data() + sizeof(effecterId) + sizeof(compEffecterCnt) +
               hdrSize,
           &stateField, sizeof(stateField));

    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = decode_set_state_effecter_states_req(
        request, requestMsg.size() - hdrSize, &retEffecterId,
        &retCompEffecterCnt, retStateField.data());

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
    const struct pldm_msg* msg = NULL;

    auto rc = decode_set_state_effecter_states_req(msg, sizeof(*msg), NULL,
                                                   NULL, NULL);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(SetStateEffecterStates, testBadDecodeResponse)
{
    std::array<uint8_t, PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES>
        responseMsg{};

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = decode_set_state_effecter_states_resp(response,
                                                    responseMsg.size(), NULL);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(GetPDR, testGoodEncodeResponse)
{
    uint8_t completionCode = 0;
    uint32_t nextRecordHndl = 0x12;
    uint32_t nextDataTransferHndl = 0x13;
    uint8_t transferFlag = PLDM_START_AND_END;
    uint16_t respCnt = 0x5;
    std::vector<uint8_t> recordData{1, 2, 3, 4, 5};
    uint8_t transferCRC = 0;

    // + size of record data and transfer CRC
    std::vector<uint8_t> responseMsg(hdrSize + PLDM_GET_PDR_MIN_RESP_BYTES +
                                     recordData.size() + 1);
    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = encode_get_pdr_resp(0, PLDM_SUCCESS, nextRecordHndl,
                                  nextDataTransferHndl, transferFlag, respCnt,
                                  recordData.data(), transferCRC, response);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    struct pldm_get_pdr_resp* resp =
        reinterpret_cast<struct pldm_get_pdr_resp*>(response->payload);

    ASSERT_EQ(completionCode, resp->completion_code);
    ASSERT_EQ(nextRecordHndl, resp->next_record_handle);
    ASSERT_EQ(nextDataTransferHndl, resp->next_data_transfer_handle);
    ASSERT_EQ(transferFlag, resp->transfer_flag);
    ASSERT_EQ(respCnt, resp->response_count);
    ASSERT_EQ(0,
              memcmp(recordData.data(), resp->record_data, recordData.size()));
}

TEST(GetPDR, testBadEncodeResponse)
{
    uint32_t nextRecordHndl = 0x12;
    uint32_t nextDataTransferHndl = 0x13;
    uint8_t transferFlag = PLDM_START_AND_END;
    uint16_t respCnt = 0x5;
    std::vector<uint8_t> recordData{1, 2, 3, 4, 5};
    uint8_t transferCRC = 0;

    auto rc = encode_get_pdr_resp(0, PLDM_SUCCESS, nextRecordHndl,
                                  nextDataTransferHndl, transferFlag, respCnt,
                                  recordData.data(), transferCRC, nullptr);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(GetPDR, testGoodDecodeRequest)
{
    std::array<uint8_t, hdrSize + PLDM_GET_PDR_REQ_BYTES> requestMsg{};

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

    auto req = reinterpret_cast<pldm_msg*>(requestMsg.data());
    struct pldm_get_pdr_req* request =
        reinterpret_cast<struct pldm_get_pdr_req*>(req->payload);

    request->record_handle = recordHndl;
    request->data_transfer_handle = dataTransferHndl;
    request->transfer_op_flag = transferOpFlag;
    request->request_count = requestCnt;
    request->record_change_number = recordChangeNum;

    auto rc = decode_get_pdr_req(
        req, requestMsg.size() - hdrSize, &retRecordHndl, &retDataTransferHndl,
        &retTransferOpFlag, &retRequestCnt, &retRecordChangeNum);

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
    auto req = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = decode_get_pdr_req(req, requestMsg.size(), NULL, NULL, NULL, NULL,
                                 NULL);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(GetPDR, testEncodeRequest)
{
    uint32_t record_handle = 0x32;
    uint32_t data_transfer_handle = 0x11;
    uint8_t transfer_operation_flag = PLDM_GET_FIRSTPART;
    uint16_t request_count = 0x5;
    uint16_t record_change_number = 0;

    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_PDR_REQ_BYTES>
        requestMsg{};
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = encode_get_pdr_req(0, record_handle, data_transfer_handle,
                                 transfer_operation_flag, request_count,
                                 record_change_number, request);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(0,
              memcmp(request->payload, &record_handle, sizeof(record_handle)));
    ASSERT_EQ(0, memcmp(request->payload + sizeof(record_handle),
                        &data_transfer_handle, sizeof(data_transfer_handle)));
    ASSERT_EQ(0, memcmp(request->payload + sizeof(record_handle) +
                            sizeof(data_transfer_handle),
                        &transfer_operation_flag,
                        sizeof(transfer_operation_flag)));
    ASSERT_EQ(0, memcmp(request->payload + sizeof(record_handle) +
                            sizeof(data_transfer_handle) +
                            sizeof(transfer_operation_flag),
                        &request_count, sizeof(request_count)));
    ASSERT_EQ(0, memcmp(request->payload + sizeof(record_handle) +
                            sizeof(data_transfer_handle) +
                            sizeof(transfer_operation_flag) +
                            sizeof(request_count),
                        &record_change_number, sizeof(record_change_number)));
}

TEST(GetPDR, testGoodDecodeResponse)
{
    // + size of record data and transfer CRC
    std::array<uint8_t, PLDM_GET_PDR_MIN_RESP_BYTES + 5 + 1> responseMsg{};

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

    memcpy(responseMsg.data(), &completion_code, sizeof(completion_code));
    memcpy(responseMsg.data() + sizeof(completion_code), &next_record_handle,
           sizeof(next_record_handle));
    memcpy(responseMsg.data() + sizeof(completion_code) +
               sizeof(next_record_handle),
           &next_data_transfer_handle, sizeof(next_data_transfer_handle));
    memcpy(responseMsg.data() + sizeof(completion_code) +
               sizeof(next_record_handle) + sizeof(next_data_transfer_handle),
           &transfer_flag, sizeof(transfer_flag));
    memcpy(responseMsg.data() + sizeof(completion_code) +
               sizeof(next_record_handle) + sizeof(next_data_transfer_handle) +
               sizeof(transfer_flag),
           &response_count, sizeof(response_count));
    memcpy(responseMsg.data() + sizeof(completion_code) +
               sizeof(next_record_handle) + sizeof(next_data_transfer_handle) +
               sizeof(transfer_flag) + sizeof(response_count),
           record_data.data(), record_data.size());
    memcpy(responseMsg.data() + sizeof(completion_code) +
               sizeof(next_record_handle) + sizeof(next_data_transfer_handle) +
               sizeof(transfer_flag) + sizeof(response_count) +
               record_data.size(),
           &transfer_crc, sizeof(transfer_crc));

    auto rc = decode_get_pdr_resp(responseMsg.data(), responseMsg.size(),
                                  &retcompletion_code, &retnext_record_handle,
                                  &retnext_data_transfer_handle,
                                  &rettransfer_flag, &retresponse_count,
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

    auto rc = decode_get_pdr_resp(responseMsg.data(), responseMsg.size(), NULL,
                                  NULL, NULL, NULL, NULL, NULL, NULL);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}
