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
