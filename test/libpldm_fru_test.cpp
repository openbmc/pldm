#include <string.h>

#include <array>

#include "libpldm/base.h"
#include "libpldm/fru.h"

#include <gtest/gtest.h>

TEST(GetFruRecordTableMetadata, testGoodEncodeRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr)> requestMsg{};
    auto requestPtr = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_get_fru_record_table_metadata_req(0, requestPtr);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(requestPtr->hdr.request, PLDM_REQUEST);
    ASSERT_EQ(requestPtr->hdr.instance_id, 0);
    ASSERT_EQ(requestPtr->hdr.type, PLDM_FRU);
    ASSERT_EQ(requestPtr->hdr.command, PLDM_GET_FRU_RECORD_TABLE_METADATA);
}

TEST(GetFruRecordTableMetadata, testBadEncodeRequest)
{
    auto rc = encode_get_fru_record_table_metadata_req(0, NULL);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
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
    response->fru_table_maximum_size = 0x1234ABCD;
    response->fru_table_length = 0x56781234;
    response->total_record_set_identifiers = 0x34EF;
    response->total_table_records = 0xEEEF;
    response->checksum = 0x6543FA71;

    uint8_t completion_code = 0xFF;
    uint8_t fru_data_major_version = 0x00;
    uint8_t fru_data_minor_version = 0x00;
    uint32_t fru_table_maximum_size = 0x00000000;
    uint32_t fru_table_length = 0x00000000;
    uint16_t total_record_set_identifiers = 0x0000;
    uint16_t total_table_records = 0x0000;
    uint32_t checksum = 0x00000000;

    auto rc = decode_get_fru_record_table_metadata_resp(
        responsePtr, payload_length, &completion_code, &fru_data_major_version,
        &fru_data_minor_version, &fru_table_maximum_size, &fru_table_length,
        &total_record_set_identifiers, &total_table_records, &checksum);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completion_code, PLDM_SUCCESS);
    ASSERT_EQ(fru_data_major_version, 0x12);
    ASSERT_EQ(fru_data_minor_version, 0x21);
    ASSERT_EQ(fru_table_maximum_size, 0x1234ABCD);
    ASSERT_EQ(fru_table_length, 0x56781234);
    ASSERT_EQ(total_record_set_identifiers, 0x34EF);
    ASSERT_EQ(total_table_records, 0xEEEF);
    ASSERT_EQ(checksum, 0x6543FA71);

    response->fru_data_major_version = 0x00;
    response->fru_data_minor_version = 0x00;
    response->fru_table_maximum_size = 0x00000000;
    response->fru_table_length = 0x00000000;
    response->total_record_set_identifiers = 0x0000;
    response->total_table_records = 0x0000;
    response->checksum = 0x00000000;
    fru_data_major_version = 0x00;
    fru_data_minor_version = 0x00;
    fru_table_maximum_size = 0x00000000;
    fru_table_length = 0x00000000;
    total_record_set_identifiers = 0x0000;
    total_table_records = 0x0000;
    checksum = 0x00000000;
    response->completion_code = PLDM_ERROR_INVALID_LENGTH;
    rc = decode_get_fru_record_table_metadata_resp(
        responsePtr, payload_length, &completion_code, &fru_data_major_version,
        &fru_data_minor_version, &fru_table_maximum_size, &fru_table_length,
        &total_record_set_identifiers, &total_table_records, &checksum);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(responsePtr->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(responsePtr->hdr.instance_id, 0);
    ASSERT_EQ(responsePtr->hdr.type, PLDM_FRU);
    ASSERT_EQ(responsePtr->hdr.command, PLDM_GET_FRU_RECORD_TABLE_METADATA);
    ASSERT_EQ(completion_code, PLDM_ERROR_INVALID_LENGTH);
    ASSERT_EQ(fru_data_major_version, 0x00);
    ASSERT_EQ(fru_data_minor_version, 0x00);
    ASSERT_EQ(fru_table_maximum_size, 0x00000000);
    ASSERT_EQ(fru_table_length, 0x00000000);
    ASSERT_EQ(total_record_set_identifiers, 0x0000);
    ASSERT_EQ(total_table_records, 0x0000);
    ASSERT_EQ(checksum, 0x00000000);
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
    response->fru_table_maximum_size = 0x1234ABCD;
    response->fru_table_length = 0x56781234;
    response->total_record_set_identifiers = 0x34EF;
    response->total_table_records = 0xEEEF;
    response->checksum = 0x6543FA71;

    uint8_t completion_code = 0xFF;
    uint8_t fru_data_major_version = 0x00;
    uint8_t fru_data_minor_version = 0x00;
    uint32_t fru_table_maximum_size = 0x00000000;
    uint32_t fru_table_length = 0x00000000;
    uint16_t total_record_set_identifiers = 0x0000;
    uint16_t total_table_records = 0x0000;
    uint32_t checksum = 0x00000000;

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
    uint32_t fru_table_maximum_size = 0x1234ABCD;
    uint32_t fru_table_length = 0x56781234;
    uint16_t total_record_set_identifiers = 0x34EF;
    uint16_t total_table_records = 0xEEEF;
    uint32_t checksum = 0x6543FA71;

    auto rc = encode_get_fru_record_table_metadata_resp(
        0, completion_code, fru_data_major_version, fru_data_minor_version,
        fru_table_maximum_size, fru_table_length, total_record_set_identifiers,
        total_table_records, checksum, responsePtr);

    auto response = reinterpret_cast<pldm_get_fru_record_table_metadata_resp*>(
        responsePtr->payload);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(responsePtr->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(responsePtr->hdr.instance_id, 0);
    ASSERT_EQ(responsePtr->hdr.type, PLDM_FRU);
    ASSERT_EQ(responsePtr->hdr.command, PLDM_GET_FRU_RECORD_TABLE_METADATA);
    ASSERT_EQ(response->completion_code, PLDM_SUCCESS);
    ASSERT_EQ(response->fru_data_major_version, 0x12);
    ASSERT_EQ(response->fru_data_minor_version, 0x21);
    ASSERT_EQ(response->fru_table_maximum_size, 0x1234ABCD);
    ASSERT_EQ(response->fru_table_length, 0x56781234);
    ASSERT_EQ(response->total_record_set_identifiers, 0x34EF);
    ASSERT_EQ(response->total_table_records, 0xEEEF);
    ASSERT_EQ(response->checksum, 0x6543FA71);

    response->fru_data_major_version = 0;
    response->fru_data_major_version = 0x00;
    response->fru_data_minor_version = 0x00;
    response->fru_table_maximum_size = 0x00000000;
    response->fru_table_length = 0x00000000;
    response->total_record_set_identifiers = 0x0000;
    response->total_table_records = 0x0000;
    response->checksum = 0x00000000;
    completion_code = PLDM_ERROR_INVALID_DATA;
    rc = encode_get_fru_record_table_metadata_resp(
        0, completion_code, fru_data_major_version, fru_data_minor_version,
        fru_table_maximum_size, fru_table_length, total_record_set_identifiers,
        total_table_records, checksum, responsePtr);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(responsePtr->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(responsePtr->hdr.instance_id, 0);
    ASSERT_EQ(responsePtr->hdr.type, PLDM_FRU);
    ASSERT_EQ(responsePtr->hdr.command, PLDM_GET_FRU_RECORD_TABLE_METADATA);
    ASSERT_EQ(completion_code, PLDM_ERROR_INVALID_DATA);
    ASSERT_EQ(response->fru_data_major_version, 0x00);
    ASSERT_EQ(response->fru_data_minor_version, 0x00);
    ASSERT_EQ(response->fru_table_maximum_size, 0x00000000);
    ASSERT_EQ(response->fru_table_length, 0x00000000);
    ASSERT_EQ(response->total_record_set_identifiers, 0x0000);
    ASSERT_EQ(response->total_table_records, 0x0000);
    ASSERT_EQ(response->checksum, 0x00000000);
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
    uint32_t fru_table_maximum_size = 0x1234ABCD;
    uint32_t fru_table_length = 0x56781234;
    uint16_t total_record_set_identifiers = 0x34EF;
    uint16_t total_table_records = 0xEEEF;
    uint32_t checksum = 0x6543FA71;

    auto rc = encode_get_fru_record_table_metadata_resp(
        0, completion_code, fru_data_major_version, fru_data_minor_version,
        fru_table_maximum_size, fru_table_length, total_record_set_identifiers,
        total_table_records, checksum, NULL);

    auto response = reinterpret_cast<pldm_get_fru_record_table_metadata_resp*>(
        responsePtr->payload);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    ASSERT_EQ(completion_code, PLDM_SUCCESS);
    ASSERT_EQ(response->fru_data_major_version, 0x00);
    ASSERT_EQ(response->fru_data_minor_version, 0x00);
    ASSERT_EQ(response->fru_table_maximum_size, 0x00000000);
    ASSERT_EQ(response->fru_table_length, 0x00000000);
    ASSERT_EQ(response->total_record_set_identifiers, 0x0000);
    ASSERT_EQ(response->total_table_records, 0x0000);
    ASSERT_EQ(response->checksum, 0x00000000);
}
