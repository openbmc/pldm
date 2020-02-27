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

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(completionCode, response->payload[0]);
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

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(effecterId, request->payload[0]);
    EXPECT_EQ(compEffecterCnt, request->payload[sizeof(effecterId)]);
    EXPECT_EQ(stateField[0].set_request,
              request->payload[sizeof(effecterId) + sizeof(compEffecterCnt)]);
    EXPECT_EQ(stateField[0].effecter_state,
              request->payload[sizeof(effecterId) + sizeof(compEffecterCnt) +
                               sizeof(stateField[0].set_request)]);
    EXPECT_EQ(stateField[1].set_request,
              request->payload[sizeof(effecterId) + sizeof(compEffecterCnt) +
                               sizeof(stateField[0])]);
    EXPECT_EQ(stateField[1].effecter_state,
              request->payload[sizeof(effecterId) + sizeof(compEffecterCnt) +
                               sizeof(stateField[0]) +
                               sizeof(stateField[1].set_request)]);
}

TEST(SetStateEffecterStates, testGoodDecodeResponse)
{
    std::array<uint8_t, hdrSize + PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES>
        responseMsg{};

    uint8_t retcompletion_code = 0;

    responseMsg[hdrSize] = PLDM_SUCCESS;

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = decode_set_state_effecter_states_resp(
        response, responseMsg.size() - hdrSize, &retcompletion_code);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(PLDM_SUCCESS, retcompletion_code);
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

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(effecterId, retEffecterId);
    EXPECT_EQ(retCompEffecterCnt, compEffecterCnt);
    EXPECT_EQ(retStateField[0].set_request, stateField[0].set_request);
    EXPECT_EQ(retStateField[0].effecter_state, stateField[0].effecter_state);
    EXPECT_EQ(retStateField[1].set_request, stateField[1].set_request);
    EXPECT_EQ(retStateField[1].effecter_state, stateField[1].effecter_state);
}

TEST(SetStateEffecterStates, testBadDecodeRequest)
{
    const struct pldm_msg* msg = NULL;

    auto rc = decode_set_state_effecter_states_req(msg, sizeof(*msg), NULL,
                                                   NULL, NULL);

    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(SetStateEffecterStates, testBadDecodeResponse)
{
    std::array<uint8_t, PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES>
        responseMsg{};

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = decode_set_state_effecter_states_resp(response,
                                                    responseMsg.size(), NULL);

    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(GetPDR, testGoodEncodeResponse)
{
    uint8_t completionCode = 0;
    uint32_t nextRecordHndl = 0x12;
    uint32_t nextDataTransferHndl = 0x13;
    uint8_t transferFlag = PLDM_END;
    uint16_t respCnt = 0x5;
    std::vector<uint8_t> recordData{1, 2, 3, 4, 5};
    uint8_t transferCRC = 6;

    // + size of record data and transfer CRC
    std::vector<uint8_t> responseMsg(hdrSize + PLDM_GET_PDR_MIN_RESP_BYTES +
                                     recordData.size() + 1);
    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = encode_get_pdr_resp(0, PLDM_SUCCESS, nextRecordHndl,
                                  nextDataTransferHndl, transferFlag, respCnt,
                                  recordData.data(), transferCRC, response);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    struct pldm_get_pdr_resp* resp =
        reinterpret_cast<struct pldm_get_pdr_resp*>(response->payload);

    EXPECT_EQ(completionCode, resp->completion_code);
    EXPECT_EQ(nextRecordHndl, resp->next_record_handle);
    EXPECT_EQ(nextDataTransferHndl, resp->next_data_transfer_handle);
    EXPECT_EQ(transferFlag, resp->transfer_flag);
    EXPECT_EQ(respCnt, resp->response_count);
    EXPECT_EQ(0,
              memcmp(recordData.data(), resp->record_data, recordData.size()));
    EXPECT_EQ(*(response->payload + sizeof(pldm_get_pdr_resp) - 1 +
                recordData.size()),
              transferCRC);

    transferFlag = PLDM_START_AND_END; // No CRC in this case
    responseMsg.resize(responseMsg.size() - sizeof(transferCRC));
    rc = encode_get_pdr_resp(0, PLDM_SUCCESS, nextRecordHndl,
                             nextDataTransferHndl, transferFlag, respCnt,
                             recordData.data(), transferCRC, response);
    EXPECT_EQ(rc, PLDM_SUCCESS);
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

    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
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

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(retRecordHndl, recordHndl);
    EXPECT_EQ(retDataTransferHndl, dataTransferHndl);
    EXPECT_EQ(retTransferOpFlag, transferOpFlag);
    EXPECT_EQ(retRequestCnt, requestCnt);
    EXPECT_EQ(retRecordChangeNum, recordChangeNum);
}

TEST(GetPDR, testBadDecodeRequest)
{
    std::array<uint8_t, PLDM_GET_PDR_REQ_BYTES> requestMsg{};
    auto req = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = decode_get_pdr_req(req, requestMsg.size(), NULL, NULL, NULL, NULL,
                                 NULL);

    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(GetPDR, testGoodEncodeRequest)
{
    uint32_t record_hndl = 0;
    uint32_t data_transfer_hndl = 0;
    uint8_t transfer_op_flag = PLDM_GET_FIRSTPART;
    uint16_t request_cnt = 20;
    uint16_t record_chg_num = 0;

    std::vector<uint8_t> requestMsg(hdrSize + PLDM_GET_PDR_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = encode_get_pdr_req(0, record_hndl, data_transfer_hndl,
                                 transfer_op_flag, request_cnt, record_chg_num,
                                 request, PLDM_GET_PDR_REQ_BYTES);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    struct pldm_get_pdr_req* req =
        reinterpret_cast<struct pldm_get_pdr_req*>(request->payload);
    EXPECT_EQ(record_hndl, req->record_handle);
    EXPECT_EQ(data_transfer_hndl, req->data_transfer_handle);
    EXPECT_EQ(transfer_op_flag, req->transfer_op_flag);
    EXPECT_EQ(request_cnt, req->request_count);
    EXPECT_EQ(record_chg_num, req->record_change_number);
}

TEST(GetPDR, testBadEncodeRequest)
{
    uint32_t record_hndl = 0;
    uint32_t data_transfer_hndl = 0;
    uint8_t transfer_op_flag = PLDM_GET_FIRSTPART;
    uint16_t request_cnt = 32;
    uint16_t record_chg_num = 0;

    std::vector<uint8_t> requestMsg(hdrSize + PLDM_GET_PDR_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = encode_get_pdr_req(0, record_hndl, data_transfer_hndl,
                                 transfer_op_flag, request_cnt, record_chg_num,
                                 nullptr, PLDM_GET_PDR_REQ_BYTES);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = encode_get_pdr_req(0, record_hndl, data_transfer_hndl,
                            transfer_op_flag, request_cnt, record_chg_num,
                            request, PLDM_GET_PDR_REQ_BYTES + 1);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(GetPDR, testGoodDecodeResponse)
{
    const char* recordData = "123456789";
    uint8_t completionCode = PLDM_SUCCESS;
    uint32_t nextRecordHndl = 0;
    uint32_t nextDataTransferHndl = 0;
    uint8_t transferFlag = PLDM_END;
    constexpr uint16_t respCnt = 9;
    uint8_t transferCRC = 96;
    size_t recordDataLength = 32;
    std::array<uint8_t, hdrSize + PLDM_GET_PDR_MIN_RESP_BYTES + respCnt +
                            sizeof(transferCRC)>
        responseMsg{};

    uint8_t retCompletionCode = 0;
    uint8_t retRecordData[32] = {0};
    uint32_t retNextRecordHndl = 0;
    uint32_t retNextDataTransferHndl = 0;
    uint8_t retTransferFlag = 0;
    uint16_t retRespCnt = 0;
    uint8_t retTransferCRC = 0;

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());
    struct pldm_get_pdr_resp* resp =
        reinterpret_cast<struct pldm_get_pdr_resp*>(response->payload);
    resp->completion_code = completionCode;
    resp->next_record_handle = htole32(nextRecordHndl);
    resp->next_data_transfer_handle = htole32(nextDataTransferHndl);
    resp->transfer_flag = transferFlag;
    resp->response_count = htole16(respCnt);
    memcpy(resp->record_data, recordData, respCnt);
    response->payload[PLDM_GET_PDR_MIN_RESP_BYTES + respCnt] = transferCRC;

    auto rc = decode_get_pdr_resp(
        response, responseMsg.size() - hdrSize, &retCompletionCode,
        &retNextRecordHndl, &retNextDataTransferHndl, &retTransferFlag,
        &retRespCnt, retRecordData, recordDataLength, &retTransferCRC);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(retCompletionCode, completionCode);
    EXPECT_EQ(retNextRecordHndl, nextRecordHndl);
    EXPECT_EQ(retNextDataTransferHndl, nextDataTransferHndl);
    EXPECT_EQ(retTransferFlag, transferFlag);
    EXPECT_EQ(retRespCnt, respCnt);
    EXPECT_EQ(retTransferCRC, transferCRC);
    EXPECT_EQ(0, memcmp(recordData, resp->record_data, respCnt));
}

TEST(GetPDR, testBadDecodeResponse)
{
    const char* recordData = "123456789";
    uint8_t completionCode = PLDM_SUCCESS;
    uint32_t nextRecordHndl = 0;
    uint32_t nextDataTransferHndl = 0;
    uint8_t transferFlag = PLDM_END;
    constexpr uint16_t respCnt = 9;
    uint8_t transferCRC = 96;
    size_t recordDataLength = 32;
    std::array<uint8_t, hdrSize + PLDM_GET_PDR_MIN_RESP_BYTES + respCnt +
                            sizeof(transferCRC)>
        responseMsg{};

    uint8_t retCompletionCode = 0;
    uint8_t retRecordData[32] = {0};
    uint32_t retNextRecordHndl = 0;
    uint32_t retNextDataTransferHndl = 0;
    uint8_t retTransferFlag = 0;
    uint16_t retRespCnt = 0;
    uint8_t retTransferCRC = 0;

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());
    struct pldm_get_pdr_resp* resp =
        reinterpret_cast<struct pldm_get_pdr_resp*>(response->payload);
    resp->completion_code = completionCode;
    resp->next_record_handle = htole32(nextRecordHndl);
    resp->next_data_transfer_handle = htole32(nextDataTransferHndl);
    resp->transfer_flag = transferFlag;
    resp->response_count = htole16(respCnt);
    memcpy(resp->record_data, recordData, respCnt);
    response->payload[PLDM_GET_PDR_MIN_RESP_BYTES + respCnt] = transferCRC;

    auto rc = decode_get_pdr_resp(response, responseMsg.size() - hdrSize, NULL,
                                  NULL, NULL, NULL, NULL, NULL, 0, NULL);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_pdr_resp(
        response, responseMsg.size() - hdrSize - 1, &retCompletionCode,
        &retNextRecordHndl, &retNextDataTransferHndl, &retTransferFlag,
        &retRespCnt, retRecordData, recordDataLength, &retTransferCRC);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(SetNumericEffecterValue, testGoodDecodeRequest)
{
    std::array<uint8_t,
               hdrSize + PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES + 3>
        requestMsg{};

    uint16_t effecter_id = 32768;
    uint8_t effecter_data_size = PLDM_EFFECTER_DATA_SIZE_UINT32;
    uint32_t effecter_value = 123456789;

    uint16_t reteffecter_id;
    uint8_t reteffecter_data_size;
    uint8_t reteffecter_value[4];

    auto req = reinterpret_cast<pldm_msg*>(requestMsg.data());
    struct pldm_set_numeric_effecter_value_req* request =
        reinterpret_cast<struct pldm_set_numeric_effecter_value_req*>(
            req->payload);

    request->effecter_id = effecter_id;
    request->effecter_data_size = effecter_data_size;
    memcpy(request->effecter_value, &effecter_value, sizeof(effecter_value));

    auto rc = decode_set_numeric_effecter_value_req(
        req, requestMsg.size() - hdrSize, &reteffecter_id,
        &reteffecter_data_size, reinterpret_cast<uint8_t*>(&reteffecter_value));

    uint32_t value = *(reinterpret_cast<uint32_t*>(&reteffecter_value[0]));
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(reteffecter_id, effecter_id);
    EXPECT_EQ(reteffecter_data_size, effecter_data_size);
    EXPECT_EQ(value, effecter_value);
}

TEST(SetNumericEffecterValue, testBadDecodeRequest)
{
    std::array<uint8_t, hdrSize + PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES>
        requestMsg{};

    auto rc = decode_set_numeric_effecter_value_req(
        NULL, requestMsg.size() - hdrSize, NULL, NULL, NULL);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    uint16_t effecter_id = 0x10;
    uint8_t effecter_data_size = PLDM_EFFECTER_DATA_SIZE_UINT8;
    uint8_t effecter_value = 1;

    uint16_t reteffecter_id;
    uint8_t reteffecter_data_size;
    uint8_t reteffecter_value[4];

    auto req = reinterpret_cast<pldm_msg*>(requestMsg.data());
    struct pldm_set_numeric_effecter_value_req* request =
        reinterpret_cast<struct pldm_set_numeric_effecter_value_req*>(
            req->payload);

    request->effecter_id = effecter_id;
    request->effecter_data_size = effecter_data_size;
    memcpy(request->effecter_value, &effecter_value, sizeof(effecter_value));

    rc = decode_set_numeric_effecter_value_req(
        req, requestMsg.size() - hdrSize - 1, &reteffecter_id,
        &reteffecter_data_size, reinterpret_cast<uint8_t*>(&reteffecter_value));
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(SetNumericEffecterValue, testGoodEncodeRequest)
{
    uint16_t effecter_id = 0;
    uint8_t effecter_data_size = PLDM_EFFECTER_DATA_SIZE_UINT8;
    uint8_t effecter_value = 1;

    std::vector<uint8_t> requestMsg(
        hdrSize + PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = encode_set_numeric_effecter_value_req(
        0, effecter_id, effecter_data_size,
        reinterpret_cast<uint8_t*>(&effecter_value), request,
        PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES);
    EXPECT_EQ(rc, PLDM_SUCCESS);

    struct pldm_set_numeric_effecter_value_req* req =
        reinterpret_cast<struct pldm_set_numeric_effecter_value_req*>(
            request->payload);
    EXPECT_EQ(effecter_id, req->effecter_id);
    EXPECT_EQ(effecter_data_size, req->effecter_data_size);
    EXPECT_EQ(effecter_value,
              *(reinterpret_cast<uint8_t*>(&req->effecter_value[0])));
}

TEST(SetNumericEffecterValue, testBadEncodeRequest)
{
    std::vector<uint8_t> requestMsg(
        hdrSize + PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = encode_set_numeric_effecter_value_req(
        0, 0, 0, NULL, NULL, PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    uint16_t effecter_value;
    rc = encode_set_numeric_effecter_value_req(
        0, 0, 6, reinterpret_cast<uint8_t*>(&effecter_value), request,
        PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(SetNumericEffecterValue, testGoodDecodeResponse)
{
    std::array<uint8_t, hdrSize + PLDM_SET_NUMERIC_EFFECTER_VALUE_RESP_BYTES>
        responseMsg{};

    uint8_t completion_code = 0xA0;

    uint8_t retcompletion_code;

    memcpy(responseMsg.data() + hdrSize, &completion_code,
           sizeof(completion_code));

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = decode_set_numeric_effecter_value_resp(
        response, responseMsg.size() - hdrSize, &retcompletion_code);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(completion_code, retcompletion_code);
}

TEST(SetNumericEffecterValue, testBadDecodeResponse)
{
    std::array<uint8_t, PLDM_SET_NUMERIC_EFFECTER_VALUE_RESP_BYTES>
        responseMsg{};

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = decode_set_numeric_effecter_value_resp(response,
                                                     responseMsg.size(), NULL);

    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(SetNumericEffecterValue, testGoodEncodeResponse)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) +
                            PLDM_SET_NUMERIC_EFFECTER_VALUE_RESP_BYTES>
        responseMsg{};
    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());
    uint8_t completionCode = 0;

    auto rc = encode_set_numeric_effecter_value_resp(
        0, PLDM_SUCCESS, response, PLDM_SET_NUMERIC_EFFECTER_VALUE_RESP_BYTES);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(completionCode, response->payload[0]);
}

TEST(SetNumericEffecterValue, testBadEncodeResponse)
{
    auto rc = encode_set_numeric_effecter_value_resp(
        0, PLDM_SUCCESS, NULL, PLDM_SET_NUMERIC_EFFECTER_VALUE_RESP_BYTES);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}
