#include <string.h>

#include <array>

#include "libpldm/base.h"
#include "libpldm/fru.h"

#include <gtest/gtest.h>

constexpr auto hdrSize = sizeof(pldm_msg_hdr);

TEST(GetFruRecordTable, testGoodEncodeRequest)
{
    std::array<uint8_t,
               sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_REQ_BYTES>
        requestMsg{};
    auto requestPtr = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto request =
        reinterpret_cast<pldm_get_fru_record_table_req*>(requestPtr->payload);

    // Random value for data transfer handle and transfer operation flag
    uint32_t data_transfer_handle = 32;
    uint8_t transfer_operation_flag = PLDM_GET_FIRSTPART;
    // Invoke encode get FRU record table request api
    auto rc = encode_get_fru_record_table_req(
        0, data_transfer_handle, transfer_operation_flag, requestPtr);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(requestPtr->hdr.request, PLDM_REQUEST);
    ASSERT_EQ(requestPtr->hdr.instance_id, 0);
    ASSERT_EQ(requestPtr->hdr.type, PLDM_FRU);
    ASSERT_EQ(requestPtr->hdr.command, PLDM_GET_FRU_RECORD_TABLE);
    ASSERT_EQ(data_transfer_handle, request->data_transfer_handle);
    ASSERT_EQ(transfer_operation_flag, request->transfer_operation_flag);
}

TEST(GetFruRecordTable, testBadEncodeRequest)
{
    uint32_t data_transfer_handle = 0x0;
    uint8_t transfer_operation_flag = 0x01;

    auto rc = encode_get_fru_record_table_req(0, data_transfer_handle,
                                              transfer_operation_flag, NULL);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(GetFruRecordTable, testGoodDecodeResponse)
{
    uint8_t completion_code = PLDM_SUCCESS;
    uint32_t next_data_transfer_handle = 0x16;
    uint8_t transfer_flag = 5;
    std::vector<uint8_t> fru_record_table_data = {1, 2, 3, 4, 5, 6, 7, 8, 9};

    std::vector<uint8_t> responseMsg(sizeof(pldm_msg_hdr) +
                                     PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES +
                                     fru_record_table_data.size());

    auto responsePtr = reinterpret_cast<pldm_msg*>(responseMsg.data());
    size_t payload_length = responseMsg.size() - sizeof(pldm_msg_hdr);
    auto response =
        reinterpret_cast<pldm_get_fru_record_table_resp*>(responsePtr->payload);

    response->completion_code = completion_code;
    response->next_data_transfer_handle = next_data_transfer_handle;
    response->transfer_flag = transfer_flag;
    memcpy(response->fru_record_table_data, fru_record_table_data.data(),
           fru_record_table_data.size());

    uint8_t ret_completion_code = 0;
    uint32_t ret_next_data_transfer_handle = 0;
    uint8_t ret_transfer_flag = 0;
    std::vector<uint8_t> ret_fru_record_table_data(9, 0);
    size_t ret_fru_record_table_length = 0;

    // Invoke decode get FRU record table response api
    auto rc = decode_get_fru_record_table_resp(
        responsePtr, payload_length, &ret_completion_code,
        &ret_next_data_transfer_handle, &ret_transfer_flag,
        ret_fru_record_table_data.data(), &ret_fru_record_table_length);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completion_code, ret_completion_code);
    ASSERT_EQ(next_data_transfer_handle, ret_next_data_transfer_handle);
    ASSERT_EQ(transfer_flag, ret_transfer_flag);
    ASSERT_EQ(0, memcmp(fru_record_table_data.data(),
                        ret_fru_record_table_data.data(),
                        ret_fru_record_table_length));
    ASSERT_EQ(fru_record_table_data.size(), ret_fru_record_table_length);
}

TEST(GetFruRecordTable, testBadDecodeResponse)
{
    uint8_t completion_code = 0;
    uint32_t next_data_transfer_handle = 0;
    uint8_t transfer_flag = 0;
    std::vector<uint8_t> fru_record_table_data(9, 0);
    size_t fru_record_table_length = 0;

    std::vector<uint8_t> responseMsg(sizeof(pldm_msg_hdr) +
                                     PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES +
                                     fru_record_table_data.size());

    auto responsePtr = reinterpret_cast<pldm_msg*>(responseMsg.data());

    // Payload message is missing
    auto rc = decode_get_fru_record_table_resp(
        NULL, 0, &completion_code, &next_data_transfer_handle, &transfer_flag,
        fru_record_table_data.data(), &fru_record_table_length);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Payload length is invalid
    rc = decode_get_fru_record_table_resp(
        responsePtr, 0, &completion_code, &next_data_transfer_handle,
        &transfer_flag, fru_record_table_data.data(), &fru_record_table_length);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(GetFruRecordTable, testGoodEncodeResponse)
{
    uint8_t completion_code = 0;
    uint32_t next_data_transfer_handle = 32;
    uint8_t transfer_flag = PLDM_START_AND_END;
    std::vector<uint8_t> fru_record_table_data = {1, 2, 3, 4, 5, 6, 7, 8, 9};

    std::vector<uint8_t> responseMsg(sizeof(pldm_msg_hdr) +
                                     PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES +
                                     fru_record_table_data.size());

    auto responsePtr = reinterpret_cast<pldm_msg*>(responseMsg.data());
    auto response =
        reinterpret_cast<pldm_get_fru_record_table_resp*>(responsePtr->payload);

    // Invoke encode get FRU record table response api
    auto rc = encode_get_fru_record_table_resp(
        0, completion_code, next_data_transfer_handle, transfer_flag,
        fru_record_table_data.data(),
        sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES +
            fru_record_table_data.size(),
        responsePtr);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(responsePtr->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(responsePtr->hdr.instance_id, 0);
    ASSERT_EQ(responsePtr->hdr.type, PLDM_FRU);
    ASSERT_EQ(responsePtr->hdr.command, PLDM_GET_FRU_RECORD_TABLE);
    ASSERT_EQ(response->completion_code, PLDM_SUCCESS);
    ASSERT_EQ(response->next_data_transfer_handle, next_data_transfer_handle);
    ASSERT_EQ(response->transfer_flag, transfer_flag);
    ASSERT_EQ(0, memcmp(fru_record_table_data.data(),
                        response->fru_record_table_data,
                        fru_record_table_data.size()));
}

TEST(GetFruRecordTable, testBadEncodeResponse)
{
    uint32_t next_data_transfer_handle = 32;
    uint8_t transfer_flag = PLDM_START_AND_END;
    std::vector<uint8_t> fru_record_table_data(9, 0);

    std::vector<uint8_t> responseMsg(sizeof(pldm_msg_hdr) +
                                     PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES +
                                     fru_record_table_data.size());

    auto responsePtr = reinterpret_cast<pldm_msg*>(responseMsg.data());
    auto rc = encode_get_fru_record_table_resp(
        0, PLDM_ERROR, next_data_transfer_handle, transfer_flag,
        fru_record_table_data.data(),
        sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES +
            fru_record_table_data.size(),
        responsePtr);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(responsePtr->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(responsePtr->hdr.instance_id, 0);
    ASSERT_EQ(responsePtr->hdr.type, PLDM_FRU);
    ASSERT_EQ(responsePtr->hdr.command, PLDM_GET_FRU_RECORD_TABLE);
    ASSERT_EQ(responsePtr->payload[0], PLDM_ERROR);

    rc = encode_get_fru_record_table_resp(
        0, PLDM_SUCCESS, next_data_transfer_handle, transfer_flag,
        fru_record_table_data.data(),
        sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES +
            fru_record_table_data.size(),
        NULL);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(GetFruRecordTable, testGoodDecodeRequest)
{
    uint32_t data_transfer_handle = 31;
    uint8_t transfer_operation_flag = PLDM_GET_FIRSTPART;
    std::array<uint8_t,
               PLDM_GET_FRU_RECORD_TABLE_REQ_BYTES + sizeof(pldm_msg_hdr)>
        requestMsg{};
    auto requestPtr = reinterpret_cast<pldm_msg*>(requestMsg.data());
    size_t payload_length = requestMsg.size() - sizeof(pldm_msg_hdr);
    auto request =
        reinterpret_cast<pldm_get_fru_record_table_req*>(requestPtr->payload);

    request->data_transfer_handle = data_transfer_handle;
    request->transfer_operation_flag = transfer_operation_flag;

    uint32_t ret_data_transfer_handle = 0;
    uint8_t ret_transfer_operation_flag = 0;

    // Invoke decode get FRU record table request api
    auto rc = decode_get_fru_record_table_req(requestPtr, payload_length,
                                              &ret_data_transfer_handle,
                                              &ret_transfer_operation_flag);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(data_transfer_handle, ret_data_transfer_handle);
    ASSERT_EQ(transfer_operation_flag, ret_transfer_operation_flag);
}

TEST(GetFruRecordTable, testBadDecodeRequest)
{
    uint32_t data_transfer_handle = 0x0;
    uint8_t transfer_operation_flag = 0x01;

    std::array<uint8_t,
               sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_REQ_BYTES>
        requestMsg{};
    auto requestPtr = reinterpret_cast<pldm_msg*>(requestMsg.data());

    // Payload message is missing
    auto rc = decode_get_fru_record_table_req(NULL, 0, &data_transfer_handle,
                                              &transfer_operation_flag);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Payload length is invalid
    rc = decode_get_fru_record_table_req(requestPtr, 0, &data_transfer_handle,
                                         &transfer_operation_flag);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(GetFruRecordByOption, testGoodEncodeRequest)
{
    std::array<uint8_t,
               sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_BY_OPTION_REQ_BYTES>
        requestMsg{};
    auto requestPtr = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto request = reinterpret_cast<pldm_get_fru_record_by_option_req*>(
        requestPtr->payload);

    uint32_t data_transfer_handle = 0x0;
    uint16_t fru_table_handle = 0x1;
    uint16_t record_set_identifier = 0x0;
    uint8_t record_type = 0x01;
    uint8_t field_type = 0x01;
    uint8_t transfer_operation_flag = PLDM_GET_NEXTPART;

    auto rc = encode_get_fru_record_by_option_req(
        0, data_transfer_handle, fru_table_handle, record_set_identifier,
        record_type, field_type, transfer_operation_flag, requestPtr);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(requestPtr->hdr.request, PLDM_REQUEST);
    ASSERT_EQ(requestPtr->hdr.instance_id, 0);
    ASSERT_EQ(requestPtr->hdr.type, PLDM_FRU);
    ASSERT_EQ(requestPtr->hdr.command, PLDM_GET_FRU_RECORD_BY_OPTION);
    ASSERT_EQ(data_transfer_handle, request->data_transfer_handle);
    ASSERT_EQ(fru_table_handle, request->fru_table_handle);
    ASSERT_EQ(record_set_identifier, request->record_set_identifier);
    ASSERT_EQ(record_type, request->record_type);
    ASSERT_EQ(field_type, request->field_type);
    ASSERT_EQ(transfer_operation_flag, request->transfer_operation_flag);
}

TEST(GetFruRecordByOption, testBadEncodeRequest)
{
    uint32_t data_transfer_handle = 0x0;
    uint16_t fru_table_handle = 0x1;
    uint16_t record_set_identifier = 0x0;
    uint8_t record_type = 0x01;
    uint8_t field_type = 0x01;
    uint8_t transfer_operation_flag = PLDM_GET_NEXTPART;

    auto rc = encode_get_fru_record_by_option_req(
        0, data_transfer_handle, fru_table_handle, record_set_identifier,
        record_type, field_type, transfer_operation_flag, NULL);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(GetFruRecordByOption, testGoodDecodeResponse)
{
    uint8_t completion_code = PLDM_SUCCESS;
    uint32_t next_data_transfer_handle = 0x16;
    uint8_t transfer_flag = 5;
    std::vector<uint8_t> fru_data_structure_data = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<uint8_t> responseMsg(
        sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_BY_OPTION_MIN_RESP_BYTES +
        fru_data_structure_data.size());
    auto responsePtr = reinterpret_cast<pldm_msg*>(responseMsg.data());
    size_t payload_length = responseMsg.size() - sizeof(pldm_msg_hdr);
    auto response = reinterpret_cast<pldm_get_fru_record_by_option_resp*>(
        responsePtr->payload);

    response->completion_code = completion_code;
    response->next_data_transfer_handle = next_data_transfer_handle;
    response->transfer_flag = transfer_flag;
    memcpy(response->fru_data_structure_data, fru_data_structure_data.data(),
           fru_data_structure_data.size());

    uint8_t ret_completion_code = 0;
    uint32_t ret_next_data_transfer_handle = 0;
    uint8_t ret_transfer_flag = 0;
    std::vector<uint8_t> ret_fru_data_structure_data(9, 0);
    size_t ret_fru_data_structure_data_length = 0;

    auto rc = decode_get_fru_record_by_option_resp(
        responsePtr, payload_length, &ret_completion_code,
        &ret_next_data_transfer_handle, &ret_transfer_flag,
        ret_fru_data_structure_data.data(),
        &ret_fru_data_structure_data_length);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completion_code, ret_completion_code);
    ASSERT_EQ(next_data_transfer_handle, ret_next_data_transfer_handle);
    ASSERT_EQ(transfer_flag, ret_transfer_flag);
    ASSERT_EQ(0, memcmp(fru_data_structure_data.data(),
                        ret_fru_data_structure_data.data(),
                        ret_fru_data_structure_data_length));
    ASSERT_EQ(fru_data_structure_data.size(),
              ret_fru_data_structure_data_length);
}

TEST(GetFruRecordByOption, testBadDecodeResponse)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) +
                            PLDM_GET_FRU_RECORD_BY_OPTION_MIN_RESP_BYTES>
        responseMsg{};

    auto responsePtr = reinterpret_cast<pldm_msg*>(responseMsg.data());

    uint8_t completion_code = 0;
    uint32_t next_data_transfer_handle = 0;
    uint8_t transfer_flag = 0;
    uint8_t fru_data_structure_data = 0;
    size_t fru_data_structure_data_length = 0;

    // Payload message is missing
    auto rc = decode_get_fru_record_by_option_resp(
        NULL, 0, &completion_code, &next_data_transfer_handle, &transfer_flag,
        &fru_data_structure_data, &fru_data_structure_data_length);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Payload length is invalid
    rc = decode_get_fru_record_by_option_resp(
        responsePtr, 0, &completion_code, &next_data_transfer_handle,
        &transfer_flag, &fru_data_structure_data,
        &fru_data_structure_data_length);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(GetFruRecordByOption, testGoodEncodeResponse)
{
    uint8_t completion_code = 0;
    uint32_t next_data_transfer_handle = 32;
    uint8_t transfer_flag = PLDM_START_AND_END;
    std::vector<uint8_t> fru_data_structure_data = {1, 2, 3, 4, 5, 6, 7, 8, 9};

    std::vector<uint8_t> responseMsg(
        sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_BY_OPTION_MIN_RESP_BYTES +
        fru_data_structure_data.size());

    auto responsePtr = reinterpret_cast<pldm_msg*>(responseMsg.data());
    auto response = reinterpret_cast<pldm_get_fru_record_by_option_resp*>(
        responsePtr->payload);

    // Invoke encode get FRU record by option response api
    auto rc = encode_get_fru_record_by_option_resp(
        0, completion_code, next_data_transfer_handle, transfer_flag,
        fru_data_structure_data.data(),
        sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_BY_OPTION_MIN_RESP_BYTES +
            fru_data_structure_data.size(),
        responsePtr);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(responsePtr->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(responsePtr->hdr.instance_id, 0);
    ASSERT_EQ(responsePtr->hdr.type, PLDM_FRU);
    ASSERT_EQ(responsePtr->hdr.command, PLDM_GET_FRU_RECORD_BY_OPTION);
    ASSERT_EQ(response->completion_code, PLDM_SUCCESS);
    ASSERT_EQ(response->next_data_transfer_handle, next_data_transfer_handle);
    ASSERT_EQ(response->transfer_flag, transfer_flag);
    ASSERT_EQ(0, memcmp(fru_data_structure_data.data(),
                        response->fru_data_structure_data,
                        fru_data_structure_data.size()));
}

TEST(GetFruRecordByOption, testBadEncodeResponse)
{
    uint32_t next_data_transfer_handle = 32;
    uint8_t transfer_flag = PLDM_START_AND_END;
    std::vector<uint8_t> fru_data_structure_data(9, 0);

    std::vector<uint8_t> responseMsg(
        sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_BY_OPTION_MIN_RESP_BYTES +
        fru_data_structure_data.size());

    auto responsePtr = reinterpret_cast<pldm_msg*>(responseMsg.data());
    auto rc = encode_get_fru_record_by_option_resp(
        0, PLDM_ERROR, next_data_transfer_handle, transfer_flag,
        fru_data_structure_data.data(),
        sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_BY_OPTION_MIN_RESP_BYTES +
            fru_data_structure_data.size(),
        responsePtr);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(responsePtr->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(responsePtr->hdr.instance_id, 0);
    ASSERT_EQ(responsePtr->hdr.type, PLDM_FRU);
    ASSERT_EQ(responsePtr->hdr.command, PLDM_GET_FRU_RECORD_BY_OPTION);
    ASSERT_EQ(responsePtr->payload[0], PLDM_ERROR);

    rc = encode_get_fru_record_table_resp(
        0, PLDM_SUCCESS, next_data_transfer_handle, transfer_flag,
        fru_data_structure_data.data(),
        sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_BY_OPTION_MIN_RESP_BYTES +
            fru_data_structure_data.size(),
        NULL);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(GetFruRecordByOption, testGoodDecodeRequest)
{
    uint32_t data_transfer_handle = 31;
    uint16_t fru_table_handle = 0x0;
    uint16_t record_set_identifier = 0x1;
    uint8_t record_type = 0x0;
    uint8_t field_type = 0x0;
    uint8_t transfer_operation_flag = PLDM_GET_FIRSTPART;

    std::array<uint8_t,
               PLDM_GET_FRU_RECORD_BY_OPTION_REQ_BYTES + sizeof(pldm_msg_hdr)>
        requestMsg{};
    auto requestPtr = reinterpret_cast<pldm_msg*>(requestMsg.data());
    size_t payload_length = requestMsg.size() - sizeof(pldm_msg_hdr);
    auto request = reinterpret_cast<pldm_get_fru_record_by_option_req*>(
        requestPtr->payload);

    request->data_transfer_handle = data_transfer_handle;
    request->fru_table_handle = fru_table_handle;
    request->record_set_identifier = record_set_identifier;
    request->record_type = record_type;
    request->field_type = field_type;
    request->transfer_operation_flag = transfer_operation_flag;

    uint32_t ret_data_transfer_handle = 0;
    uint16_t ret_fru_table_handle = 0x0;
    uint16_t ret_record_set_identifier = 0x0;
    uint8_t ret_record_type = 0x0;
    uint8_t ret_field_type = 0x0;
    uint8_t ret_transfer_operation_flag = 0;

    // Invoke decode get FRU record by option request api
    auto rc = decode_get_fru_record_by_option_req(
        requestPtr, payload_length, &ret_data_transfer_handle,
        &ret_fru_table_handle, &ret_record_set_identifier, &ret_record_type,
        &ret_field_type, &ret_transfer_operation_flag);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(data_transfer_handle, ret_data_transfer_handle);
    ASSERT_EQ(fru_table_handle, ret_fru_table_handle);
    ASSERT_EQ(record_set_identifier, ret_record_set_identifier);
    ASSERT_EQ(record_type, ret_record_type);
    ASSERT_EQ(field_type, ret_field_type);
    ASSERT_EQ(transfer_operation_flag, ret_transfer_operation_flag);
}

TEST(GetFruRecordByOption, testBadDecodeRequest)
{
    uint32_t data_transfer_handle = 0x0;
    uint16_t fru_table_handle = 0x0;
    uint16_t record_set_identifier = 0x1;
    uint8_t record_type = 0x0;
    uint8_t field_type = 0x0;
    uint8_t transfer_operation_flag = 0x01;

    std::array<uint8_t,
               sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_BY_OPTION_REQ_BYTES>
        requestMsg{};
    auto requestPtr = reinterpret_cast<pldm_msg*>(requestMsg.data());

    // Payload message is missing
    auto rc = decode_get_fru_record_by_option_req(
        NULL, 0, &data_transfer_handle, &fru_table_handle,
        &record_set_identifier, &record_type, &field_type,
        &transfer_operation_flag);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Payload length is invalid
    rc = decode_get_fru_record_by_option_req(
        requestPtr, 0, &data_transfer_handle, &fru_table_handle,
        &record_set_identifier, &record_type, &field_type,
        &transfer_operation_flag);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}
