#include <string.h>

#include <array>

#include "libpldm/base.h"
#include "libpldm/fru.h"

#include <gtest/gtest.h>

TEST(GetFruRecordTableMetadata, testGoodEncodeRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr)> requestMsg{};
    auto requestPtr = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_get_fru_record_table_metadata_req(
        0, requestPtr, PLDM_GET_FRU_RECORD_TABLE_METADATA_REQ_BYTES);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(requestPtr->hdr.request, PLDM_REQUEST);
    ASSERT_EQ(requestPtr->hdr.instance_id, 0u);
    ASSERT_EQ(requestPtr->hdr.type, PLDM_FRU);
    ASSERT_EQ(requestPtr->hdr.command, PLDM_GET_FRU_RECORD_TABLE_METADATA);
}

TEST(GetFruRecordTableMetadata, testBadEncodeRequest)
{
    auto rc = encode_get_fru_record_table_metadata_req(0, NULL, 0);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    std::array<uint8_t, sizeof(pldm_msg_hdr)> requestMsg{};
    auto requestPtr = reinterpret_cast<pldm_msg*>(requestMsg.data());
    rc = encode_get_fru_record_table_metadata_req(0, requestPtr, 1);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(GetFruRecordTableMetadata, testGoodDecodeResponse)
{

    std::vector<uint8_t> responseMsg(
        sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_METADATA_RESP_BYTES);
    auto responsePtr = reinterpret_cast<pldm_msg*>(responseMsg.data());
    size_t payload_length = responseMsg.size() - sizeof(pldm_msg_hdr);
    auto response = reinterpret_cast<pldm_get_fru_record_table_metadata_resp*>(
        responsePtr->payload);

    responsePtr->hdr.request = PLDM_RESPONSE;
    responsePtr->hdr.instance_id = 0;
    responsePtr->hdr.type = PLDM_FRU;
    responsePtr->hdr.command = PLDM_GET_FRU_RECORD_TABLE_METADATA;
    response->completion_code = PLDM_SUCCESS;
    response->fru_data_major_version = 0x12;
    response->fru_data_minor_version = 0x21;
    response->fru_table_maximum_size = htole32(0x1234ABCD);
    response->fru_table_length = htole32(0x56781234);
    response->total_record_set_identifiers = htole16(0x34EF);
    response->total_table_records = htole16(0xEEEF);
    response->checksum = htole32(0x6543FA71);

    uint8_t completion_code = 0xFF;
    uint8_t fru_data_major_version = 0x00;
    uint8_t fru_data_minor_version = 0x00;
    uint32_t fru_table_maximum_size = htole32(0x00000000);
    uint32_t fru_table_length = htole32(0x00000000);
    uint16_t total_record_set_identifiers = htole16(0x0000);
    uint16_t total_table_records = htole16(0x0000);
    uint32_t checksum = htole32(0x00000000);

    auto rc = decode_get_fru_record_table_metadata_resp(
        responsePtr, payload_length, &completion_code, &fru_data_major_version,
        &fru_data_minor_version, &fru_table_maximum_size, &fru_table_length,
        &total_record_set_identifiers, &total_table_records, &checksum);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completion_code, PLDM_SUCCESS);
    ASSERT_EQ(fru_data_major_version, 0x12u);
    ASSERT_EQ(fru_data_minor_version, 0x21u);
    ASSERT_EQ(fru_table_maximum_size, 0x1234ABCDu);
    ASSERT_EQ(fru_table_length, 0x56781234u);
    ASSERT_EQ(total_record_set_identifiers, 0x34EFu);
    ASSERT_EQ(total_table_records, 0xEEEFu);
    ASSERT_EQ(checksum, 0x6543FA71u);

    response->fru_data_major_version = 0x00;
    response->fru_data_minor_version = 0x00;
    response->fru_table_maximum_size = htole32(0x00000000);
    response->fru_table_length = htole32(0x00000000);
    response->total_record_set_identifiers = htole16(0x0000);
    response->total_table_records = htole16(0x0000);
    response->checksum = htole32(0x00000000);
    fru_data_major_version = 0x00;
    fru_data_minor_version = 0x00;
    fru_table_maximum_size = htole32(0x00000000);
    fru_table_length = htole32(0x00000000);
    total_record_set_identifiers = htole16(0x0000);
    total_table_records = htole16(0x0000);
    checksum = htole32(0x00000000);
    response->completion_code = PLDM_ERROR_INVALID_LENGTH;
    rc = decode_get_fru_record_table_metadata_resp(
        responsePtr, payload_length, &completion_code, &fru_data_major_version,
        &fru_data_minor_version, &fru_table_maximum_size, &fru_table_length,
        &total_record_set_identifiers, &total_table_records, &checksum);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(responsePtr->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(responsePtr->hdr.instance_id, 0u);
    ASSERT_EQ(responsePtr->hdr.type, PLDM_FRU);
    ASSERT_EQ(responsePtr->hdr.command, PLDM_GET_FRU_RECORD_TABLE_METADATA);
    ASSERT_EQ(completion_code, PLDM_ERROR_INVALID_LENGTH);
    ASSERT_EQ(fru_data_major_version, 0x00u);
    ASSERT_EQ(fru_data_minor_version, 0x00u);
    ASSERT_EQ(fru_table_maximum_size, 0x00000000u);
    ASSERT_EQ(fru_table_length, 0x00000000u);
    ASSERT_EQ(total_record_set_identifiers, 0x0000u);
    ASSERT_EQ(total_table_records, 0x0000u);
    ASSERT_EQ(checksum, 0x00000000u);
}

TEST(GetFruRecordTableMetadata, testBadDecodeResponse)
{
    std::vector<uint8_t> responseMsg(
        sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_METADATA_RESP_BYTES);
    auto responsePtr = reinterpret_cast<pldm_msg*>(responseMsg.data());
    size_t payload_length = responseMsg.size() - sizeof(pldm_msg_hdr);
    auto response = reinterpret_cast<pldm_get_fru_record_table_metadata_resp*>(
        responsePtr->payload);

    response->completion_code = PLDM_SUCCESS;
    response->fru_data_major_version = 0x12;
    response->fru_data_minor_version = 0x21;
    response->fru_table_maximum_size = htole32(0x1234ABCD);
    response->fru_table_length = htole32(0x56781234);
    response->total_record_set_identifiers = htole16(0x34EF);
    response->total_table_records = htole16(0xEEEF);
    response->checksum = htole32(0x6543FA71);

    uint8_t completion_code = 0xFF;
    uint8_t fru_data_major_version = 0x00;
    uint8_t fru_data_minor_version = 0x00;
    uint32_t fru_table_maximum_size = htole32(0x00000000);
    uint32_t fru_table_length = htole32(0x00000000);
    uint16_t total_record_set_identifiers = htole16(0x0000);
    uint16_t total_table_records = htole16(0x0000);
    uint32_t checksum = htole32(0x00000000);

    auto rc = decode_get_fru_record_table_metadata_resp(
        responsePtr, PLDM_GET_FRU_RECORD_TABLE_METADATA_RESP_BYTES + 2,
        &completion_code, &fru_data_major_version, &fru_data_minor_version,
        &fru_table_maximum_size, &fru_table_length,
        &total_record_set_identifiers, &total_table_records, &checksum);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    rc = decode_get_fru_record_table_metadata_resp(
        responsePtr, payload_length, &completion_code, NULL,
        &fru_data_minor_version, &fru_table_maximum_size, &fru_table_length,
        &total_record_set_identifiers, &total_table_records, &checksum);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_fru_record_table_metadata_resp(
        responsePtr, payload_length, &completion_code, &fru_data_major_version,
        NULL, &fru_table_maximum_size, &fru_table_length,
        &total_record_set_identifiers, &total_table_records, &checksum);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_fru_record_table_metadata_resp(
        responsePtr, payload_length, &completion_code, &fru_data_major_version,
        &fru_data_minor_version, NULL, &fru_table_length,
        &total_record_set_identifiers, &total_table_records, &checksum);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_fru_record_table_metadata_resp(
        responsePtr, payload_length, &completion_code, &fru_data_major_version,
        &fru_data_minor_version, &fru_table_maximum_size, NULL,
        &total_record_set_identifiers, &total_table_records, &checksum);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_fru_record_table_metadata_resp(
        responsePtr, payload_length, &completion_code, &fru_data_major_version,
        &fru_data_minor_version, &fru_table_maximum_size, &fru_table_length,
        NULL, &total_table_records, &checksum);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_fru_record_table_metadata_resp(
        responsePtr, payload_length, &completion_code, &fru_data_major_version,
        &fru_data_minor_version, &fru_table_maximum_size, &fru_table_length,
        &total_record_set_identifiers, NULL, &checksum);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_fru_record_table_metadata_resp(
        responsePtr, payload_length, &completion_code, &fru_data_major_version,
        &fru_data_minor_version, &fru_table_maximum_size, &fru_table_length,
        &total_record_set_identifiers, &total_table_records, NULL);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(GetFruRecordTableMetadata, testGoodEncodeResponse)
{

    std::array<uint8_t, sizeof(pldm_msg_hdr) +
                            PLDM_GET_FRU_RECORD_TABLE_METADATA_RESP_BYTES>
        responseMsg{};
    auto responsePtr = reinterpret_cast<pldm_msg*>(responseMsg.data());

    responsePtr->hdr.request = PLDM_RESPONSE;
    responsePtr->hdr.instance_id = 0;
    responsePtr->hdr.type = PLDM_FRU;
    responsePtr->hdr.command = PLDM_GET_FRU_RECORD_TABLE_METADATA;

    uint8_t completion_code = PLDM_SUCCESS;
    uint8_t fru_data_major_version = 0x12;
    uint8_t fru_data_minor_version = 0x21;
    uint32_t fru_table_maximum_size = htole32(0x1234ABCD);
    uint32_t fru_table_length = htole32(0x56781234);
    uint16_t total_record_set_identifiers = htole16(0x34EF);
    uint16_t total_table_records = htole16(0xEEEF);
    uint32_t checksum = htole32(0x6543FA71);

    auto rc = encode_get_fru_record_table_metadata_resp(
        0, completion_code, fru_data_major_version, fru_data_minor_version,
        fru_table_maximum_size, fru_table_length, total_record_set_identifiers,
        total_table_records, checksum, responsePtr);

    auto response = reinterpret_cast<pldm_get_fru_record_table_metadata_resp*>(
        responsePtr->payload);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(responsePtr->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(responsePtr->hdr.instance_id, 0u);
    ASSERT_EQ(responsePtr->hdr.type, PLDM_FRU);
    ASSERT_EQ(responsePtr->hdr.command, PLDM_GET_FRU_RECORD_TABLE_METADATA);
    ASSERT_EQ(response->completion_code, PLDM_SUCCESS);
    ASSERT_EQ(response->fru_data_major_version, 0x12u);
    ASSERT_EQ(response->fru_data_minor_version, 0x21u);
    ASSERT_EQ(response->fru_table_maximum_size, 0x1234ABCDu);
    ASSERT_EQ(response->fru_table_length, 0x56781234u);
    ASSERT_EQ(response->total_record_set_identifiers, 0x34EFu);
    ASSERT_EQ(response->total_table_records, 0xEEEFu);
    ASSERT_EQ(response->checksum, 0x6543FA71u);

    response->fru_data_major_version = 0;
    response->fru_data_major_version = 0x00;
    response->fru_data_minor_version = 0x00;
    response->fru_table_maximum_size = htole32(0x00000000);
    response->fru_table_length = htole32(0x00000000);
    response->total_record_set_identifiers = htole16(0x0000);
    response->total_table_records = htole16(0x0000);
    response->checksum = htole32(0x00000000);
    completion_code = PLDM_ERROR_INVALID_DATA;
    rc = encode_get_fru_record_table_metadata_resp(
        0, completion_code, fru_data_major_version, fru_data_minor_version,
        fru_table_maximum_size, fru_table_length, total_record_set_identifiers,
        total_table_records, checksum, responsePtr);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(responsePtr->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(responsePtr->hdr.instance_id, 0u);
    ASSERT_EQ(responsePtr->hdr.type, PLDM_FRU);
    ASSERT_EQ(responsePtr->hdr.command, PLDM_GET_FRU_RECORD_TABLE_METADATA);
    ASSERT_EQ(completion_code, PLDM_ERROR_INVALID_DATA);
    ASSERT_EQ(response->fru_data_major_version, 0x00u);
    ASSERT_EQ(response->fru_data_minor_version, 0x00u);
    ASSERT_EQ(response->fru_table_maximum_size, 0x00000000u);
    ASSERT_EQ(response->fru_table_length, 0x00000000u);
    ASSERT_EQ(response->total_record_set_identifiers, 0x0000u);
    ASSERT_EQ(response->total_table_records, 0x0000u);
    ASSERT_EQ(response->checksum, 0x00000000u);
}

TEST(GetFruRecordTableMetadata, testBadEncodeResponse)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) +
                            PLDM_GET_FRU_RECORD_TABLE_METADATA_RESP_BYTES>
        responseMsg{};
    auto responsePtr = reinterpret_cast<pldm_msg*>(responseMsg.data());

    uint8_t completion_code = PLDM_SUCCESS;
    uint8_t fru_data_major_version = 0x12;
    uint8_t fru_data_minor_version = 0x21;
    uint32_t fru_table_maximum_size = htole32(0x1234ABCD);
    uint32_t fru_table_length = htole32(0x56781234);
    uint16_t total_record_set_identifiers = htole16(0x34EF);
    uint16_t total_table_records = htole16(0xEEEF);
    uint32_t checksum = htole32(0x6543FA71);

    auto rc = encode_get_fru_record_table_metadata_resp(
        0, completion_code, fru_data_major_version, fru_data_minor_version,
        fru_table_maximum_size, fru_table_length, total_record_set_identifiers,
        total_table_records, checksum, NULL);

    auto response = reinterpret_cast<pldm_get_fru_record_table_metadata_resp*>(
        responsePtr->payload);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    ASSERT_EQ(completion_code, PLDM_SUCCESS);
    ASSERT_EQ(response->fru_data_major_version, 0x00u);
    ASSERT_EQ(response->fru_data_minor_version, 0x00u);
    ASSERT_EQ(response->fru_table_maximum_size, 0x00000000u);
    ASSERT_EQ(response->fru_table_length, 0x00000000u);
    ASSERT_EQ(response->total_record_set_identifiers, 0x0000u);
    ASSERT_EQ(response->total_table_records, 0x0000u);
    ASSERT_EQ(response->checksum, 0x00000000u);
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

    request->data_transfer_handle = htole32(data_transfer_handle);
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
    uint8_t transfer_operation_flag = PLDM_GET_FIRSTPART;

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

TEST(GetFruRecordTable, testGoodEncodeResponse)
{
    uint8_t completion_code = 0;
    uint32_t next_data_transfer_handle = 32;
    uint8_t transfer_flag = PLDM_START_AND_END;

    std::vector<uint8_t> responseMsg(sizeof(pldm_msg_hdr) +
                                     PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES);

    auto responsePtr = reinterpret_cast<pldm_msg*>(responseMsg.data());
    auto response =
        reinterpret_cast<pldm_get_fru_record_table_resp*>(responsePtr->payload);

    // Invoke encode get FRU record table response api
    auto rc = encode_get_fru_record_table_resp(0, completion_code,
                                               next_data_transfer_handle,
                                               transfer_flag, responsePtr);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(responsePtr->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(responsePtr->hdr.instance_id, 0u);
    ASSERT_EQ(responsePtr->hdr.type, PLDM_FRU);
    ASSERT_EQ(responsePtr->hdr.command, PLDM_GET_FRU_RECORD_TABLE);
    ASSERT_EQ(response->completion_code, PLDM_SUCCESS);
    ASSERT_EQ(le32toh(response->next_data_transfer_handle),
              next_data_transfer_handle);
    ASSERT_EQ(response->transfer_flag, transfer_flag);
}

TEST(GetFruRecordTable, testBadEncodeResponse)
{
    uint32_t next_data_transfer_handle = 32;
    uint8_t transfer_flag = PLDM_START_AND_END;

    std::vector<uint8_t> responseMsg(sizeof(pldm_msg_hdr) +
                                     PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES);

    auto responsePtr = reinterpret_cast<pldm_msg*>(responseMsg.data());
    auto rc = encode_get_fru_record_table_resp(
        0, PLDM_ERROR, next_data_transfer_handle, transfer_flag, responsePtr);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(responsePtr->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(responsePtr->hdr.instance_id, 0u);
    ASSERT_EQ(responsePtr->hdr.type, PLDM_FRU);
    ASSERT_EQ(responsePtr->hdr.command, PLDM_GET_FRU_RECORD_TABLE);
    ASSERT_EQ(responsePtr->payload[0], PLDM_ERROR);

    rc = encode_get_fru_record_table_resp(
        0, PLDM_SUCCESS, next_data_transfer_handle, transfer_flag, nullptr);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

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
        0, data_transfer_handle, transfer_operation_flag, requestPtr,
        requestMsg.size() - sizeof(pldm_msg_hdr));

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(requestPtr->hdr.request, PLDM_REQUEST);
    ASSERT_EQ(requestPtr->hdr.instance_id, 0u);
    ASSERT_EQ(requestPtr->hdr.type, PLDM_FRU);
    ASSERT_EQ(requestPtr->hdr.command, PLDM_GET_FRU_RECORD_TABLE);
    ASSERT_EQ(le32toh(data_transfer_handle), request->data_transfer_handle);
    ASSERT_EQ(transfer_operation_flag, request->transfer_operation_flag);
}

TEST(GetFruRecordTable, testBadEncodeRequest)

{
    uint32_t data_transfer_handle = 0x0;
    uint8_t transfer_operation_flag = PLDM_GET_FIRSTPART;

    std::array<uint8_t,
               sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_REQ_BYTES>
        requestMsg{};
    auto rc = encode_get_fru_record_table_req(
        0, data_transfer_handle, transfer_operation_flag, NULL,
        requestMsg.size() - sizeof(pldm_msg_hdr));

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(GetFruRecordTable, testGoodDecodeResponse)
{
    uint8_t completion_code = PLDM_SUCCESS;
    uint32_t next_data_transfer_handle = 0x16;
    uint8_t transfer_flag = PLDM_START_AND_END;
    std::vector<uint8_t> fru_record_table_data = {1, 2, 3, 4, 5, 6, 7, 8, 9};

    std::vector<uint8_t> responseMsg(sizeof(pldm_msg_hdr) +
                                     PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES +
                                     fru_record_table_data.size());

    auto responsePtr = reinterpret_cast<pldm_msg*>(responseMsg.data());
    size_t payload_length = responseMsg.size() - sizeof(pldm_msg_hdr);
    auto response =
        reinterpret_cast<pldm_get_fru_record_table_resp*>(responsePtr->payload);

    response->completion_code = completion_code;
    response->next_data_transfer_handle = htole32(next_data_transfer_handle);
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
    uint8_t transfer_flag = PLDM_START_AND_END;
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

TEST(FruRecordTableCalc, testGoodTable)
{
    uint16_t recordSetIdA = 10;
    uint8_t recordTypeA = PLDM_FRU_RECORD_TYPE_GENERAL;
    uint8_t numberFruFeildA = 2;
    uint8_t encodingTypeA = PLDM_FRU_ENCODING_ASCII;
    uint8_t typeA1 = PLDM_FRU_FIELD_TYPE_CHASSIS;
    const std::string valueA1("E980");
    uint8_t lengthA1 = valueA1.size();
    uint8_t typeA2 = PLDM_FRU_FIELD_TYPE_MODEL;
    const std::string valueA2("007");
    uint8_t lengthA2 = valueA2.size();
    uint16_t recordSetIdB = 11;
    uint8_t recordTypeB = PLDM_FRU_RECORD_TYPE_GENERAL;
    uint8_t numberFruFeildB = 2;
    uint8_t encodingTypeB = PLDM_FRU_ENCODING_ASCII;
    uint8_t typeB1 = PLDM_FRU_FIELD_TYPE_MANUFAC;
    const std::string valueB1("IBM");
    uint8_t lengthB1 = valueB1.size();
    uint8_t typeB2 = PLDM_FRU_FIELD_TYPE_VENDOR;
    const std::string valueB2("ISDL");
    uint8_t lengthB2 = valueB2.size();
    size_t fruTableSize =
        sizeof(recordSetIdA) + sizeof(recordTypeA) + sizeof(numberFruFeildA) +
        sizeof(encodingTypeA) + sizeof(typeA1) + valueA1.size() +
        sizeof(lengthA1) + sizeof(typeA2) + valueA2.size() + sizeof(lengthA2) +
        sizeof(recordSetIdB) + sizeof(recordTypeB) + sizeof(numberFruFeildB) +
        sizeof(encodingTypeB) + sizeof(typeB1) + valueB1.size() +
        sizeof(lengthB1) + sizeof(typeB2) + valueB2.size() + sizeof(lengthB2);
    std::array<uint8_t, 100> fruRecordTable{};
    auto fruRecordTableData1 =
        reinterpret_cast<struct pldm_fru_record_data_format*>(
            fruRecordTable.data());
    fruRecordTableData1->record_set_id = recordSetIdA;
    fruRecordTableData1->record_type = recordTypeA;
    fruRecordTableData1->num_fru_fields = numberFruFeildA;
    fruRecordTableData1->encoding_type = encodingTypeA;
    auto tlvs1 = reinterpret_cast<struct pldm_fru_record_tlv*>(
        fruRecordTableData1->tlvs);
    tlvs1->type = typeA1;
    tlvs1->length = lengthA1;
    memcpy(tlvs1->value, &valueA1, lengthA1);
    auto tlvs2 =
        reinterpret_cast<struct pldm_fru_record_tlv*>(tlvs1->value + lengthA1);
    tlvs2->type = typeA2;
    tlvs2->length = lengthA2;
    memcpy(tlvs2->value, &valueA2, lengthA2);

    auto fruRecordTableData2 =
        reinterpret_cast<struct pldm_fru_record_data_format*>(tlvs2->value +
                                                              lengthA2);
    fruRecordTableData2->record_set_id = recordSetIdB;
    fruRecordTableData2->record_type = recordTypeB;
    fruRecordTableData2->num_fru_fields = numberFruFeildB;
    fruRecordTableData2->encoding_type = encodingTypeB;
    auto tlvs3 = reinterpret_cast<struct pldm_fru_record_tlv*>(
        fruRecordTableData2->tlvs);
    tlvs3->type = typeB1;
    tlvs3->length = lengthB1;
    memcpy(tlvs3->value, &valueB1, lengthB1);
    auto tlvs4 =
        reinterpret_cast<struct pldm_fru_record_tlv*>(tlvs3->value + lengthB1);
    tlvs4->type = typeB2;
    tlvs4->length = lengthB2;
    memcpy(tlvs4->value, &valueB2, lengthB2);

    size_t recordDataSize{0};
    auto rc = fru_record_data_size_calc(
        reinterpret_cast<const uint8_t*>(fruRecordTable.data()), 0,
        fruTableSize, &recordDataSize);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(recordDataSize,
              sizeof(recordSetIdA) + sizeof(recordTypeA) +
                  sizeof(numberFruFeildA) + sizeof(encodingTypeA) +
                  sizeof(typeA1) + valueA1.size() + sizeof(lengthA1) +
                  sizeof(typeA2) + valueA2.size() + sizeof(lengthA2));
    auto nextFruRecord = reinterpret_cast<struct pldm_fru_record_data_format*>(
        fruRecordTable.data() + recordDataSize);
    ASSERT_EQ(nextFruRecord->record_set_id, recordSetIdB);
    size_t newRecordDataSize{0};
    rc = fru_record_data_size_calc(
        reinterpret_cast<const uint8_t*>(fruRecordTable.data()), recordDataSize,
        fruTableSize, &newRecordDataSize);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(newRecordDataSize + recordDataSize, fruTableSize);
}

TEST(FruRecordTableCalc, testBadTable)
{
    size_t recordDataSize{0};
    auto rc = fru_record_data_size_calc(nullptr, 0, 10, &recordDataSize);
    ASSERT_EQ(rc, PLDM_ERROR);

    std::array<uint8_t, 10> fruTable{};
    rc = fru_record_data_size_calc(
        reinterpret_cast<const uint8_t*>(fruTable.data()), fruTable.size(),
        fruTable.size(), &recordDataSize);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}