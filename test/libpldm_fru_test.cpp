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
    uint32_t next_data_transfer_handle = 0x32323232;
    uint8_t transfer_flag = 5;

    std::vector<uint8_t> responseMsg(sizeof(pldm_msg_hdr) +
                                     PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES +
                                     next_data_transfer_handle + transfer_flag);
    auto responsePtr = reinterpret_cast<pldm_msg*>(responseMsg.data());
    size_t payload_length = responseMsg.size() - sizeof(pldm_msg_hdr);
    auto response =
        reinterpret_cast<pldm_get_fru_record_table_resp*>(responsePtr->payload);

    response->completion_code = completion_code;
    response->next_data_transfer_handle = next_data_transfer_handle;
    response->transfer_flag = transfer_flag;

    size_t fru_record_offset = sizeof(completion_code) +
                               sizeof(next_data_transfer_handle) +
                               sizeof(transfer_flag);

    uint8_t ret_completion_code = 0;
    uint32_t ret_next_data_transfer_handle = 0;
    uint8_t ret_transfer_flag = 0;
    size_t ret_fru_record_offset = 0;

    // Invoke decode get FRU record table response api
    auto rc = decode_get_fru_record_table_resp(
        responsePtr, payload_length, &ret_completion_code,
        &ret_next_data_transfer_handle, &ret_transfer_flag,
        &ret_fru_record_offset);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completion_code, ret_completion_code);
    ASSERT_EQ(next_data_transfer_handle, ret_next_data_transfer_handle);
    ASSERT_EQ(transfer_flag, ret_transfer_flag);
    ASSERT_EQ(fru_record_offset, ret_fru_record_offset);
}

TEST(GetFruRecordTable, testBadDecodeResponse)
{
    std::array<uint8_t,
               sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES>
        responseMsg{};

    auto responsePtr = reinterpret_cast<pldm_msg*>(responseMsg.data());

    uint8_t completion_code = 0;
    uint32_t next_data_transfer_handle = 0;
    uint8_t transfer_flag = 0;
    size_t fru_record_offset = 0;

    // Payload message is missing
    auto rc = decode_get_fru_record_table_resp(
        NULL, 0, &completion_code, &next_data_transfer_handle, &transfer_flag,
        &fru_record_offset);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Payload length is invalid
    rc = decode_get_fru_record_table_resp(responsePtr, 0, &completion_code,
                                          &next_data_transfer_handle,
                                          &transfer_flag, &fru_record_offset);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(GetFruRecordTable, testGoodEncodeResponse)
{
    uint8_t completion_code = 0;
    uint32_t next_data_transfer_handle = 32;
    uint8_t transfer_flag = PLDM_START_AND_END;

    std::array<uint8_t,
               sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES>
        responseMsg{};
    auto responsePtr = reinterpret_cast<pldm_msg*>(responseMsg.data());
    auto response =
        reinterpret_cast<pldm_get_fru_record_table_resp*>(responsePtr->payload);

    // Invoke encode get FRU record table response api
    auto rc = encode_get_fru_record_table_resp(0, completion_code,
                                               next_data_transfer_handle,
                                               transfer_flag, responsePtr);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(responsePtr->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(responsePtr->hdr.instance_id, 0);
    ASSERT_EQ(responsePtr->hdr.type, PLDM_FRU);
    ASSERT_EQ(responsePtr->hdr.command, PLDM_GET_FRU_RECORD_TABLE);
    ASSERT_EQ(response->completion_code, PLDM_SUCCESS);
    ASSERT_EQ(response->next_data_transfer_handle, next_data_transfer_handle);
    ASSERT_EQ(response->transfer_flag, transfer_flag);
}

TEST(GetFruRecordTable, testBadEncodeResponse)
{
    uint32_t next_data_transfer_handle = 32;
    uint8_t transfer_flag = PLDM_START_AND_END;

    std::array<uint8_t,
               sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES>
        responseMsg{};
    auto responsePtr = reinterpret_cast<pldm_msg*>(responseMsg.data());
    auto rc = encode_get_fru_record_table_resp(
        0, PLDM_ERROR, next_data_transfer_handle, transfer_flag, responsePtr);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(responsePtr->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(responsePtr->hdr.instance_id, 0);
    ASSERT_EQ(responsePtr->hdr.type, PLDM_FRU);
    ASSERT_EQ(responsePtr->hdr.command, PLDM_GET_FRU_RECORD_TABLE);
    ASSERT_EQ(responsePtr->payload[0], PLDM_ERROR);

    rc = encode_get_fru_record_table_resp(
        0, PLDM_SUCCESS, next_data_transfer_handle, transfer_flag, NULL);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}
