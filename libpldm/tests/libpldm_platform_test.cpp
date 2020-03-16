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
    uint16_t effecterIdLE = htole16(effecterId);
    uint8_t compEffecterCnt = 0x2;

    std::array<set_effecter_state_field, 8> stateField{};
    stateField[0] = {PLDM_REQUEST_SET, 3};
    stateField[1] = {PLDM_REQUEST_SET, 4};

    uint16_t retEffecterId = 0;
    uint8_t retCompEffecterCnt = 0;

    std::array<set_effecter_state_field, 8> retStateField{};

    memcpy(requestMsg.data() + hdrSize, &effecterIdLE, sizeof(effecterIdLE));
    memcpy(requestMsg.data() + sizeof(effecterIdLE) + hdrSize, &compEffecterCnt,
           sizeof(compEffecterCnt));
    memcpy(requestMsg.data() + sizeof(effecterIdLE) + sizeof(compEffecterCnt) +
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
    EXPECT_EQ(nextRecordHndl, le32toh(resp->next_record_handle));
    EXPECT_EQ(nextDataTransferHndl, le32toh(resp->next_data_transfer_handle));
    EXPECT_EQ(transferFlag, resp->transfer_flag);
    EXPECT_EQ(respCnt, le16toh(resp->response_count));
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
    uint16_t recordChangeNum = 0x01;

    uint32_t retRecordHndl = 0;
    uint32_t retDataTransferHndl = 0;
    uint8_t retTransferOpFlag = 0;
    uint16_t retRequestCnt = 0;
    uint16_t retRecordChangeNum = 0;

    auto req = reinterpret_cast<pldm_msg*>(requestMsg.data());
    struct pldm_get_pdr_req* request =
        reinterpret_cast<struct pldm_get_pdr_req*>(req->payload);

    request->record_handle = htole32(recordHndl);
    request->data_transfer_handle = htole32(dataTransferHndl);
    request->transfer_op_flag = transferOpFlag;
    request->request_count = htole16(requestCnt);
    request->record_change_number = htole16(recordChangeNum);

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
    EXPECT_EQ(record_hndl, le32toh(req->record_handle));
    EXPECT_EQ(data_transfer_hndl, le32toh(req->data_transfer_handle));
    EXPECT_EQ(transfer_op_flag, req->transfer_op_flag);
    EXPECT_EQ(request_cnt, le16toh(req->request_count));
    EXPECT_EQ(record_chg_num, le16toh(req->record_change_number));
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

    request->effecter_id = htole16(effecter_id);
    request->effecter_data_size = effecter_data_size;
    uint32_t effecter_value_le = htole32(effecter_value);
    memcpy(request->effecter_value, &effecter_value_le,
           sizeof(effecter_value_le));

    auto rc = decode_set_numeric_effecter_value_req(
        req, requestMsg.size() - hdrSize, &reteffecter_id,
        &reteffecter_data_size, reteffecter_value);

    uint32_t value = le32toh(*(reinterpret_cast<uint32_t*>(reteffecter_value)));
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

TEST(GetStateSensorReadings, testGoodEncodeResponse)
{
    std::array<uint8_t, hdrSize + PLDM_GET_STATE_SENSOR_READINGS_RESP_BYTES>
        responseMsg{};

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());
    uint8_t completionCode = 0;
    uint8_t comp_sensorCnt = 0x2;

    std::array<get_sensor_state_field, 2> stateField{};
    stateField[0] = {ENABLED, NORMAL, WARNING, UNKNOWN};
    stateField[1] = {FAILED, UPPERFATAL, UPPERCRITICAL, FATAL};

    auto rc = encode_get_state_sensor_readings_resp(
        0, PLDM_SUCCESS, comp_sensorCnt, stateField.data(), response);

    struct pldm_get_state_sensor_readings_resp* resp =
        reinterpret_cast<struct pldm_get_state_sensor_readings_resp*>(
            response->payload);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(completionCode, resp->completion_code);
    EXPECT_EQ(comp_sensorCnt, resp->comp_sensor_count);
    EXPECT_EQ(stateField[0].sensor_op_state, resp->field->sensor_op_state);
    EXPECT_EQ(stateField[0].present_state, resp->field->present_state);
    EXPECT_EQ(stateField[0].previous_state, resp->field->previous_state);
    EXPECT_EQ(stateField[0].event_state, resp->field->event_state);
    EXPECT_EQ(stateField[1].sensor_op_state, resp->field[1].sensor_op_state);
    EXPECT_EQ(stateField[1].present_state, resp->field[1].present_state);
    EXPECT_EQ(stateField[1].previous_state, resp->field[1].previous_state);
    EXPECT_EQ(stateField[1].event_state, resp->field[1].event_state);
}

TEST(GetStateSensorReadings, testBadEncodeResponse)
{
    auto rc = encode_get_state_sensor_readings_resp(0, PLDM_SUCCESS, 0, nullptr,
                                                    nullptr);

    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(GetStateSensorReadings, testGoodDecodeResponse)
{
    std::array<uint8_t, hdrSize + PLDM_GET_STATE_SENSOR_READINGS_RESP_BYTES>
        responseMsg{};

    uint8_t completionCode = 0;
    uint8_t comp_sensorCnt = 2;

    std::array<get_sensor_state_field, 2> stateField{};
    stateField[0] = {DISABLED, UNKNOWN, UNKNOWN, UNKNOWN};
    stateField[1] = {ENABLED, LOWERFATAL, LOWERCRITICAL, WARNING};

    uint8_t retcompletion_code = 0;
    uint8_t retcomp_sensorCnt = 2;
    std::array<get_sensor_state_field, 2> retstateField{};

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());
    struct pldm_get_state_sensor_readings_resp* resp =
        reinterpret_cast<struct pldm_get_state_sensor_readings_resp*>(
            response->payload);

    resp->completion_code = completionCode;
    resp->comp_sensor_count = comp_sensorCnt;
    memcpy(resp->field, &stateField,
           (sizeof(get_sensor_state_field) * comp_sensorCnt));

    auto rc = decode_get_state_sensor_readings_resp(
        response, responseMsg.size() - hdrSize, &retcompletion_code,
        &retcomp_sensorCnt, retstateField.data());

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(completionCode, retcompletion_code);
    EXPECT_EQ(comp_sensorCnt, retcomp_sensorCnt);
    EXPECT_EQ(stateField[0].sensor_op_state, retstateField[0].sensor_op_state);
    EXPECT_EQ(stateField[0].present_state, retstateField[0].present_state);
    EXPECT_EQ(stateField[0].previous_state, retstateField[0].previous_state);
    EXPECT_EQ(stateField[0].event_state, retstateField[0].event_state);
    EXPECT_EQ(stateField[1].sensor_op_state, retstateField[1].sensor_op_state);
    EXPECT_EQ(stateField[1].present_state, retstateField[1].present_state);
    EXPECT_EQ(stateField[1].previous_state, retstateField[1].previous_state);
    EXPECT_EQ(stateField[1].event_state, retstateField[1].event_state);
}

TEST(GetStateSensorReadings, testBadDecodeResponse)
{
    std::array<uint8_t, hdrSize + PLDM_GET_STATE_SENSOR_READINGS_RESP_BYTES>
        responseMsg{};

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = decode_get_state_sensor_readings_resp(
        response, responseMsg.size() - hdrSize, nullptr, nullptr, nullptr);

    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    uint8_t completionCode = 0;
    uint8_t comp_sensorCnt = 1;

    std::array<get_sensor_state_field, 1> stateField{};
    stateField[0] = {ENABLED, UPPERFATAL, UPPERCRITICAL, WARNING};

    uint8_t retcompletion_code = 0;
    uint8_t retcomp_sensorCnt = 0;
    std::array<get_sensor_state_field, 1> retstateField{};

    struct pldm_get_state_sensor_readings_resp* resp =
        reinterpret_cast<struct pldm_get_state_sensor_readings_resp*>(
            response->payload);

    resp->completion_code = completionCode;
    resp->comp_sensor_count = comp_sensorCnt;
    memcpy(resp->field, &stateField,
           (sizeof(get_sensor_state_field) * comp_sensorCnt));

    rc = decode_get_state_sensor_readings_resp(
        response, responseMsg.size() - hdrSize, &retcompletion_code,
        &retcomp_sensorCnt, retstateField.data());

    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(GetStateSensorReadings, testGoodEncodeRequest)
{
    std::array<uint8_t, hdrSize + PLDM_GET_STATE_SENSOR_READINGS_REQ_BYTES>
        requestMsg{};

    uint16_t sensorId = 0xAB;
    bitfield8_t sensorRearm;
    sensorRearm.byte = 0x03;

    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_get_state_sensor_readings_req(0, sensorId, sensorRearm, 0,
                                                   request);

    struct pldm_get_state_sensor_readings_req* req =
        reinterpret_cast<struct pldm_get_state_sensor_readings_req*>(
            request->payload);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(sensorId, le16toh(req->sensor_id));
    EXPECT_EQ(sensorRearm.byte, req->sensor_rearm.byte);
}

TEST(GetStateSensorReadings, testBadEncodeRequest)
{
    bitfield8_t sensorRearm;
    sensorRearm.byte = 0x0;

    auto rc =
        encode_get_state_sensor_readings_req(0, 0, sensorRearm, 0, nullptr);

    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(GetStateSensorReadings, testGoodDecodeRequest)
{
    std::array<uint8_t, hdrSize + PLDM_GET_STATE_SENSOR_READINGS_REQ_BYTES>
        requestMsg{};

    uint16_t sensorId = 0xCD;
    bitfield8_t sensorRearm;
    sensorRearm.byte = 0x10;

    uint16_t retsensorId;
    bitfield8_t retsensorRearm;
    uint8_t retreserved;

    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    struct pldm_get_state_sensor_readings_req* req =
        reinterpret_cast<struct pldm_get_state_sensor_readings_req*>(
            request->payload);

    req->sensor_id = htole16(sensorId);
    req->sensor_rearm.byte = sensorRearm.byte;

    auto rc = decode_get_state_sensor_readings_req(
        request, requestMsg.size() - hdrSize, &retsensorId, &retsensorRearm,
        &retreserved);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(sensorId, retsensorId);
    EXPECT_EQ(sensorRearm.byte, retsensorRearm.byte);
    EXPECT_EQ(0, retreserved);
}

TEST(GetStateSensorReadings, testBadDecodeRequest)
{
    std::array<uint8_t, hdrSize + PLDM_GET_STATE_SENSOR_READINGS_REQ_BYTES>
        requestMsg{};

    auto rc = decode_get_state_sensor_readings_req(
        nullptr, requestMsg.size() - hdrSize, nullptr, nullptr, nullptr);

    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    uint16_t sensorId = 0x11;
    bitfield8_t sensorRearm;
    sensorRearm.byte = 0x04;

    uint16_t retsensorId;
    bitfield8_t retsensorRearm;
    uint8_t retreserved;

    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    struct pldm_get_state_sensor_readings_req* req =
        reinterpret_cast<struct pldm_get_state_sensor_readings_req*>(
            request->payload);

    req->sensor_id = htole16(sensorId);
    req->sensor_rearm.byte = sensorRearm.byte;

    rc = decode_get_state_sensor_readings_req(
        request, requestMsg.size() - hdrSize - 1, &retsensorId, &retsensorRearm,
        &retreserved);

    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(GetNumericEffecterValue, testGoodEncodeRequest)
{
    std::vector<uint8_t> requestMsg(hdrSize +
                                    PLDM_GET_NUMERIC_EFFECTER_VALUE_REQ_BYTES);

    uint16_t effecter_id = 0xAB01;

    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = encode_get_numeric_effecter_value_req(0, effecter_id, request);

    struct pldm_get_numeric_effecter_value_req* req =
        reinterpret_cast<struct pldm_get_numeric_effecter_value_req*>(
            request->payload);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(effecter_id, le16toh(req->effecter_id));
}

TEST(GetNumericEffecterValue, testBadEncodeRequest)
{
    std::vector<uint8_t> requestMsg(
        hdrSize + PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES);

    auto rc = encode_get_numeric_effecter_value_req(0, 0, nullptr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(GetNumericEffecterValue, testGoodDecodeRequest)
{
    std::array<uint8_t, hdrSize + PLDM_GET_NUMERIC_EFFECTER_VALUE_REQ_BYTES>
        requestMsg{};

    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    struct pldm_get_numeric_effecter_value_req* req =
        reinterpret_cast<struct pldm_get_numeric_effecter_value_req*>(
            request->payload);

    uint16_t effecter_id = 0x12AB;
    req->effecter_id = htole16(effecter_id);

    uint16_t reteffecter_id;

    auto rc = decode_get_numeric_effecter_value_req(
        request, requestMsg.size() - hdrSize, &reteffecter_id);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(effecter_id, reteffecter_id);
}

TEST(GetNumericEffecterValue, testBadDecodeRequest)
{
    std::array<uint8_t, hdrSize + PLDM_GET_NUMERIC_EFFECTER_VALUE_REQ_BYTES>
        requestMsg{};

    auto rc = decode_get_numeric_effecter_value_req(
        nullptr, requestMsg.size() - hdrSize, nullptr);

    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    struct pldm_set_numeric_effecter_value_req* req =
        reinterpret_cast<struct pldm_set_numeric_effecter_value_req*>(
            request->payload);

    uint16_t effecter_id = 0x1A;
    req->effecter_id = htole16(effecter_id);
    uint16_t reteffecter_id;

    rc = decode_get_numeric_effecter_value_req(
        request, requestMsg.size() - hdrSize - 1, &reteffecter_id);

    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(GetNumericEffecterValue, testGoodEncodeResponse)
{
    uint8_t completionCode = 0;
    uint8_t effecter_dataSize = PLDM_EFFECTER_DATA_SIZE_UINT8;
    uint8_t effecter_operState = EFFECTER_OPER_STATE_ENABLED_NOUPDATEPENDING;
    uint8_t pendingValue = 0x12;
    uint8_t presentValue = 0xAB;

    std::array<uint8_t,
               hdrSize + PLDM_GET_NUMERIC_EFFECTER_VALUE_MIN_RESP_BYTES>
        responseMsg{};
    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = encode_get_numeric_effecter_value_resp(
        0, completionCode, effecter_dataSize, effecter_operState,
        reinterpret_cast<uint8_t*>(&pendingValue),
        reinterpret_cast<uint8_t*>(&presentValue), response,
        responseMsg.size() - hdrSize);

    struct pldm_get_numeric_effecter_value_resp* resp =
        reinterpret_cast<struct pldm_get_numeric_effecter_value_resp*>(
            response->payload);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(effecter_dataSize, resp->effecter_data_size);
    EXPECT_EQ(effecter_operState, resp->effecter_oper_state);
    EXPECT_EQ(pendingValue, *(reinterpret_cast<uint8_t*>(
                                &resp->pending_and_present_values[0])));
    EXPECT_EQ(presentValue, *(reinterpret_cast<uint8_t*>(
                                &resp->pending_and_present_values[1])));
}

TEST(GetNumericEffecterValue, testBadEncodeResponse)
{
    std::array<uint8_t,
               hdrSize + PLDM_GET_NUMERIC_EFFECTER_VALUE_MIN_RESP_BYTES + 2>
        responseMsg{};
    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    uint8_t pendingValue = 0x01;
    uint8_t presentValue = 0x02;

    auto rc = encode_get_numeric_effecter_value_resp(
        0, PLDM_SUCCESS, 0, 0, nullptr, nullptr, nullptr,
        responseMsg.size() - hdrSize);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = encode_get_numeric_effecter_value_resp(
        0, PLDM_SUCCESS, 6, 9, reinterpret_cast<uint8_t*>(&pendingValue),
        reinterpret_cast<uint8_t*>(&presentValue), response,
        responseMsg.size() - hdrSize);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    uint8_t effecter_dataSize = PLDM_EFFECTER_DATA_SIZE_UINT8;
    uint8_t effecter_operState = EFFECTER_OPER_STATE_FAILED;

    rc = encode_get_numeric_effecter_value_resp(
        0, PLDM_SUCCESS, effecter_dataSize, effecter_operState,
        reinterpret_cast<uint8_t*>(&pendingValue),
        reinterpret_cast<uint8_t*>(&presentValue), response,
        responseMsg.size() - hdrSize);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(GetNumericEffecterValue, testGoodDecodeResponse)
{
    std::array<uint8_t,
               hdrSize + PLDM_GET_NUMERIC_EFFECTER_VALUE_MIN_RESP_BYTES + 2>
        responseMsg{};

    uint8_t completionCode = 0;
    uint8_t effecter_dataSize = PLDM_EFFECTER_DATA_SIZE_UINT16;
    uint8_t effecter_operState = EFFECTER_OPER_STATE_ENABLED_NOUPDATEPENDING;
    uint16_t pendingValue = 0x4321;
    uint16_t presentValue = 0xDCBA;

    uint8_t retcompletionCode;
    uint8_t reteffecter_dataSize;
    uint8_t reteffecter_operState;
    uint8_t retpendingValue[2];
    uint8_t retpresentValue[2];

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());
    struct pldm_get_numeric_effecter_value_resp* resp =
        reinterpret_cast<struct pldm_get_numeric_effecter_value_resp*>(
            response->payload);

    resp->completion_code = completionCode;
    resp->effecter_data_size = effecter_dataSize;
    resp->effecter_oper_state = effecter_operState;

    uint16_t pendingValue_le = htole16(pendingValue);
    memcpy(resp->pending_and_present_values, &pendingValue_le,
           sizeof(pendingValue_le));
    uint16_t presentValue_le = htole16(presentValue);
    memcpy(&resp->pending_and_present_values[2], &presentValue_le,
           sizeof(presentValue_le));

    auto rc = decode_get_numeric_effecter_value_resp(
        response, responseMsg.size() - hdrSize, &retcompletionCode,
        &reteffecter_dataSize, &reteffecter_operState, retpendingValue,
        retpresentValue);

    uint16_t retpending_value =
        le16toh(*(reinterpret_cast<uint16_t*>(retpendingValue)));
    uint16_t retpresent_value =
        le16toh(*(reinterpret_cast<uint16_t*>(retpresentValue)));

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(completionCode, retcompletionCode);
    EXPECT_EQ(effecter_dataSize, reteffecter_dataSize);
    EXPECT_EQ(effecter_operState, reteffecter_operState);
    EXPECT_EQ(pendingValue, retpending_value);
    EXPECT_EQ(presentValue, retpresent_value);
}

TEST(GetNumericEffecterValue, testBadDecodeResponse)
{
    std::array<uint8_t,
               hdrSize + PLDM_GET_NUMERIC_EFFECTER_VALUE_MIN_RESP_BYTES + 6>
        responseMsg{};

    auto rc = decode_get_numeric_effecter_value_resp(
        nullptr, responseMsg.size() - hdrSize, nullptr, nullptr, nullptr,
        nullptr, nullptr);

    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    uint8_t completionCode = 0;
    uint8_t effecter_dataSize = PLDM_EFFECTER_DATA_SIZE_SINT16;
    uint8_t effecter_operState = EFFECTER_OPER_STATE_DISABLED;
    uint16_t pendingValue = 0x5678;
    uint16_t presentValue = 0xCDEF;

    uint8_t retcompletionCode;
    uint8_t reteffecter_dataSize;
    uint8_t reteffecter_operState;
    uint8_t retpendingValue[2];
    uint8_t retpresentValue[2];

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());
    struct pldm_get_numeric_effecter_value_resp* resp =
        reinterpret_cast<struct pldm_get_numeric_effecter_value_resp*>(
            response->payload);

    resp->completion_code = completionCode;
    resp->effecter_data_size = effecter_dataSize;
    resp->effecter_oper_state = effecter_operState;

    uint16_t pendingValue_le = htole16(pendingValue);
    memcpy(resp->pending_and_present_values, &pendingValue_le,
           sizeof(pendingValue_le));
    uint16_t presentValue_le = htole16(presentValue);
    memcpy(&resp->pending_and_present_values[2], &presentValue_le,
           sizeof(presentValue_le));

    rc = decode_get_numeric_effecter_value_resp(
        response, responseMsg.size() - hdrSize, &retcompletionCode,
        &reteffecter_dataSize, &reteffecter_operState, retpendingValue,
        retpresentValue);

    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}
