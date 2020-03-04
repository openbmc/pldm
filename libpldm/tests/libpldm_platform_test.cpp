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

TEST(PlatformEventMessage, testGoodStateSensorDecodeRequest)
{
    std::array<uint8_t,
               hdrSize + PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES +
                   PLDM_PLATFORM_EVENT_MESSAGE_STATE_SENSOR_STATE_REQ_BYTES>
        requestMsg{};

    size_t eventDataLength =
        PLDM_PLATFORM_EVENT_MESSAGE_STATE_SENSOR_STATE_REQ_BYTES;

    uint8_t retFormatVersion = 0;
    uint8_t retTid = 0;
    uint8_t retEventClass = 0;
    size_t retEventDataLength = 0;

    auto req = reinterpret_cast<pldm_msg*>(requestMsg.data());
    struct pldm_platform_event_message_req* request =
        reinterpret_cast<struct pldm_platform_event_message_req*>(req->payload);

    uint8_t formatVersion = 0x01;
    uint8_t tid = 0x02;
    // Sensor Event
    uint8_t eventClass = 0x00;

    request->format_version = formatVersion;
    request->tid = tid;
    request->event_class = eventClass;

    struct pldm_sensor_event_data* eventData =
        reinterpret_cast<struct pldm_sensor_event_data*>(request->event_data);

    uint16_t sensorId = 0x1234;
    uint8_t sensorEventClassType = PLDM_STATE_SENSOR_STATE;
    eventData->sensor_id = sensorId;
    eventData->sensor_event_class_type = sensorEventClassType;
    uint8_t sensorOffset = 0x02;
    uint8_t eventState = PLDM_SENSOR_INITIALIZING;
    uint8_t previousEventState = PLDM_SENSOR_DISABLED;

    struct pldm_sensor_event_state_sensor_state* eventClassData =
        reinterpret_cast<struct pldm_sensor_event_state_sensor_state*>(
            eventData->event_class);
    eventClassData->sensor_offset = sensorOffset;
    eventClassData->event_state = eventState;
    eventClassData->previous_event_state = previousEventState;

    std::array<uint8_t,
               PLDM_PLATFORM_EVENT_MESSAGE_STATE_SENSOR_STATE_REQ_BYTES>
        retEventData{};

    auto rc = decode_platform_event_message_req(
        req, requestMsg.size() - hdrSize, &retFormatVersion, &retTid,
        &retEventClass, reinterpret_cast<uint8_t*>(&retEventData),
        &retEventDataLength);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(retFormatVersion, formatVersion);
    EXPECT_EQ(retTid, tid);
    EXPECT_EQ(retEventClass, eventClass);
    EXPECT_EQ(eventDataLength, retEventDataLength);
    struct pldm_sensor_event_data* retEventDataPtr =
        reinterpret_cast<struct pldm_sensor_event_data*>(retEventData.data());
    EXPECT_EQ(sensorId, retEventDataPtr->sensor_id);
    EXPECT_EQ(sensorEventClassType, retEventDataPtr->sensor_event_class_type);
    struct pldm_sensor_event_state_sensor_state* retEventClassData =
        reinterpret_cast<struct pldm_sensor_event_state_sensor_state*>(
            retEventDataPtr->event_class);
    EXPECT_EQ(sensorOffset, retEventClassData->sensor_offset);
    EXPECT_EQ(eventState, retEventClassData->event_state);
    EXPECT_EQ(previousEventState, retEventClassData->previous_event_state);
}

TEST(PlatformEventMessage, testBadDecodeRequest)
{
    const struct pldm_msg* msg = NULL;
    std::array<uint8_t,
               hdrSize + PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES +
                   PLDM_PLATFORM_EVENT_MESSAGE_STATE_SENSOR_STATE_REQ_BYTES - 1>
        requestMsg{};
    auto req = reinterpret_cast<pldm_msg*>(requestMsg.data());
    uint8_t retFormatVersion;
    uint8_t retTid = 0;
    uint8_t retEventClass = 0;
    size_t retEventDataLength;
    std::array<uint8_t,
               PLDM_PLATFORM_EVENT_MESSAGE_STATE_SENSOR_STATE_REQ_BYTES>
        retEventData{};

    auto rc = decode_platform_event_message_req(msg, sizeof(*msg), NULL, NULL,
                                                NULL, NULL, NULL);

    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_platform_event_message_req(
        req,
        requestMsg.size() - hdrSize -
            PLDM_PLATFORM_EVENT_MESSAGE_STATE_SENSOR_STATE_REQ_BYTES,
        &retFormatVersion, &retTid, &retEventClass, retEventData.data(),
        &retEventDataLength);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(PlatformEventMessage, testGoodEncodeResponse)
{
    std::array<uint8_t,
               hdrSize + PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES +
                   PLDM_PLATFORM_EVENT_MESSAGE_STATE_SENSOR_STATE_REQ_BYTES - 1>
        responseMsg{};
    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());
    uint8_t completionCode = 0;
    uint8_t instanceId = 0x01;
    uint8_t status = 1;

    auto rc = encode_platform_event_message_resp(instanceId, PLDM_SUCCESS,
                                                 status, response);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(completionCode, response->payload[0]);
    EXPECT_EQ(status, response->payload[1]);
}

TEST(PlatformEventMessage, testBadEncodeResponse)
{
    auto rc = encode_platform_event_message_resp(0, PLDM_SUCCESS, 1, NULL);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(PlatformEventMessage, testGoodSensorOpEventDataDecodeRequest)
{
    std::array<uint8_t, PLDM_SENSOR_EVENT_SENSOR_OP_STATE_DATA_LENGTH +
                            PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES>
        eventDataArr{};
    uint16_t sensorId = 0x1234;
    uint8_t sensorEventClassType = PLDM_SENSOR_OP_STATE;

    struct pldm_sensor_event_data* eventData =
        (struct pldm_sensor_event_data*)eventDataArr.data();
    eventData->sensor_id = sensorId;
    eventData->sensor_event_class_type = sensorEventClassType;
    struct pldm_sensor_event_sensor_op_state* sensorData =
        reinterpret_cast<pldm_sensor_event_sensor_op_state*>(
            eventData->event_class);
    uint8_t presentState = PLDM_SENSOR_ENABLED;
    uint8_t previousState = PLDM_SENSOR_INITIALIZING;
    sensorData->present_op_state = presentState;
    sensorData->previous_op_state = previousState;

    std::array<uint8_t, PLDM_SENSOR_EVENT_SENSOR_OP_STATE_DATA_LENGTH>
        retSensorOpData{};

    size_t retSensorOpDataLength;
    uint16_t retSensorId = 0;
    uint8_t retSensorEventClassType;
    auto rc = decode_sensor_event_data(
        reinterpret_cast<uint8_t*>(eventData), eventDataArr.size(),
        &retSensorId, &retSensorEventClassType,
        reinterpret_cast<uint8_t*>(&retSensorOpData), &retSensorOpDataLength);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(retSensorId, sensorId);
    EXPECT_EQ(retSensorEventClassType, sensorEventClassType);
    EXPECT_EQ(retSensorOpDataLength,
              PLDM_SENSOR_EVENT_SENSOR_OP_STATE_DATA_LENGTH);
    struct pldm_sensor_event_sensor_op_state* sensorOpData =
        reinterpret_cast<pldm_sensor_event_sensor_op_state*>(
            retSensorOpData.data());
    EXPECT_EQ(sensorOpData->present_op_state, presentState);
    EXPECT_EQ(sensorOpData->previous_op_state, previousState);

    uint8_t retPresentState;
    uint8_t retPreviousState;
    rc = decode_sensor_op_data(reinterpret_cast<uint8_t*>(&retSensorOpData),
                               retSensorOpDataLength, &retPresentState,
                               &retPreviousState);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(retPresentState, presentState);
    EXPECT_EQ(retPreviousState, previousState);
}

TEST(PlatformEventMessage, testGoodSensorStateEventDataDecodeRequest)
{
    std::array<uint8_t, PLDM_SENSOR_EVENT_STATE_SENSOR_STATE_DATA_LENGTH +
                            PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES>
        eventDataArr{};
    uint16_t sensorId = 0x2345;
    uint8_t sensorEventClassType = PLDM_STATE_SENSOR_STATE;

    struct pldm_sensor_event_data* eventData =
        (struct pldm_sensor_event_data*)eventDataArr.data();
    eventData->sensor_id = sensorId;
    eventData->sensor_event_class_type = sensorEventClassType;
    struct pldm_sensor_event_state_sensor_state* sensorData =
        reinterpret_cast<pldm_sensor_event_state_sensor_state*>(
            eventData->event_class);
    uint8_t sensorOffset = 0x02;
    uint8_t eventState = PLDM_SENSOR_SHUTTINGDOWN;
    uint8_t previousEventState = PLDM_SENSOR_INTEST;
    sensorData->sensor_offset = sensorOffset;
    sensorData->event_state = eventState;
    sensorData->previous_event_state = previousEventState;

    std::array<uint8_t, PLDM_SENSOR_EVENT_STATE_SENSOR_STATE_DATA_LENGTH>
        retSensorStateData{};

    size_t retSensorStateDataLength;
    uint16_t retSensorId = 0;
    uint8_t retSensorEventClassType;
    auto rc = decode_sensor_event_data(
        reinterpret_cast<uint8_t*>(eventData), eventDataArr.size(),
        &retSensorId, &retSensorEventClassType,
        reinterpret_cast<uint8_t*>(&retSensorStateData),
        &retSensorStateDataLength);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(retSensorId, sensorId);
    EXPECT_EQ(retSensorEventClassType, sensorEventClassType);
    EXPECT_EQ(retSensorStateDataLength,
              PLDM_SENSOR_EVENT_STATE_SENSOR_STATE_DATA_LENGTH);
    struct pldm_sensor_event_state_sensor_state* sensorStateData =
        reinterpret_cast<pldm_sensor_event_state_sensor_state*>(
            retSensorStateData.data());
    EXPECT_EQ(sensorStateData->sensor_offset, sensorOffset);
    EXPECT_EQ(sensorStateData->event_state, eventState);
    EXPECT_EQ(sensorStateData->previous_event_state, previousEventState);

    uint8_t retSensorOffset;
    uint8_t retEventState;
    uint8_t retPreviousState;
    rc = decode_state_sensor_data(
        reinterpret_cast<uint8_t*>(&retSensorStateData),
        retSensorStateDataLength, &retSensorOffset, &retEventState,
        &retPreviousState);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(retSensorOffset, sensorOffset);
    EXPECT_EQ(retEventState, eventState);
    EXPECT_EQ(retPreviousState, previousEventState);
}

TEST(PlatformEventMessage, testGoodNumericSensorEventDataDecodeRequest)
{
    std::array<uint8_t, PLDM_SENSOR_EVENT_NUMERIC_SENSOR_STATE_MIN_DATA_LENGTH +
                            PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES>
        eventDataArr{};
    uint16_t sensorId = 0x3456;
    uint8_t sensorEventClassType = PLDM_NUMERIC_SENSOR_STATE;

    struct pldm_sensor_event_data* eventData =
        (struct pldm_sensor_event_data*)eventDataArr.data();
    eventData->sensor_id = sensorId;
    eventData->sensor_event_class_type = sensorEventClassType;
    struct pldm_sensor_event_numeric_sensor_state* sensorData =
        reinterpret_cast<pldm_sensor_event_numeric_sensor_state*>(
            eventData->event_class);

    uint8_t eventState = PLDM_SENSOR_SHUTTINGDOWN;
    uint8_t previousEventState = PLDM_SENSOR_INTEST;
    uint8_t sensorDataSize = PLDM_SENSOR_DATA_SIZE_UINT8;
    uint8_t presentReading = 0xAB;
    sensorData->event_state = eventState;
    sensorData->previous_event_state = previousEventState;
    sensorData->sensor_data_size = sensorDataSize;
    sensorData->present_reading[0] = presentReading;

    std::array<uint8_t, PLDM_SENSOR_EVENT_NUMERIC_SENSOR_STATE_MIN_DATA_LENGTH>
        retSensorNumericData{};

    size_t retSensorNumericDataLength;
    uint16_t retSensorId = 0;
    uint8_t retSensorEventClassType;
    auto rc = decode_sensor_event_data(
        reinterpret_cast<uint8_t*>(eventData), eventDataArr.size(),
        &retSensorId, &retSensorEventClassType,
        reinterpret_cast<uint8_t*>(&retSensorNumericData),
        &retSensorNumericDataLength);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(retSensorId, sensorId);
    EXPECT_EQ(retSensorEventClassType, sensorEventClassType);
    EXPECT_EQ(retSensorNumericDataLength,
              PLDM_SENSOR_EVENT_NUMERIC_SENSOR_STATE_MIN_DATA_LENGTH);
    struct pldm_sensor_event_numeric_sensor_state* sensorNumericData =
        reinterpret_cast<pldm_sensor_event_numeric_sensor_state*>(
            retSensorNumericData.data());
    EXPECT_EQ(sensorNumericData->event_state, eventState);
    EXPECT_EQ(sensorNumericData->previous_event_state, previousEventState);
    EXPECT_EQ(sensorNumericData->sensor_data_size, sensorDataSize);
    EXPECT_EQ(sensorNumericData->present_reading[0], presentReading);

    uint8_t retSensorDataSize;
    uint8_t retEventState;
    uint8_t retPreviousState;
    uint32_t retPresentReading;
    rc = decode_numeric_sensor_data(
        reinterpret_cast<uint8_t*>(&retSensorNumericData),
        retSensorNumericDataLength, &retEventState, &retPreviousState,
        &retSensorDataSize, &retPresentReading);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(retSensorDataSize, sensorDataSize);
    EXPECT_EQ(retEventState, eventState);
    EXPECT_EQ(retPreviousState, previousEventState);
    EXPECT_EQ(retPresentReading, presentReading);
}
