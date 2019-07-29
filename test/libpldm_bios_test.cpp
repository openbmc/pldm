#include <string.h>

#include <array>

#include "libpldm/base.h"
#include "libpldm/bios.h"

#include <gtest/gtest.h>

constexpr auto hdrSize = sizeof(pldm_msg_hdr);

TEST(GetDateTime, testEncodeRequest)
{
    pldm_msg request{};

    auto rc = encode_get_date_time_req(0, &request);
    ASSERT_EQ(rc, PLDM_SUCCESS);
}

TEST(GetDateTime, testEncodeResponse)
{
    uint8_t completionCode = 0;
    uint8_t seconds = 50;
    uint8_t minutes = 20;
    uint8_t hours = 5;
    uint8_t day = 23;
    uint8_t month = 11;
    uint16_t year = 2019;

    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_DATE_TIME_RESP_BYTES>
        responseMsg{};

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = encode_get_date_time_resp(0, PLDM_SUCCESS, seconds, minutes,
                                        hours, day, month, year, response);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completionCode, response->payload[0]);

    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]),
                        &seconds, sizeof(seconds)));
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]) +
                            sizeof(seconds),
                        &minutes, sizeof(minutes)));
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]) +
                            sizeof(seconds) + sizeof(minutes),
                        &hours, sizeof(hours)));
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]) +
                            sizeof(seconds) + sizeof(minutes) + sizeof(hours),
                        &day, sizeof(day)));
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]) +
                            sizeof(seconds) + sizeof(minutes) + sizeof(hours) +
                            sizeof(day),
                        &month, sizeof(month)));
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]) +
                            sizeof(seconds) + sizeof(minutes) + sizeof(hours) +
                            sizeof(day) + sizeof(month),
                        &year, sizeof(year)));
}

TEST(GetDateTime, testDecodeResponse)
{
    std::array<uint8_t, hdrSize + PLDM_GET_DATE_TIME_RESP_BYTES> responseMsg{};

    uint8_t completionCode = 0;

    uint8_t seconds = 55;
    uint8_t minutes = 2;
    uint8_t hours = 8;
    uint8_t day = 9;
    uint8_t month = 7;
    uint16_t year = 2020;

    uint8_t retSeconds = 0;
    uint8_t retMinutes = 0;
    uint8_t retHours = 0;
    uint8_t retDay = 0;
    uint8_t retMonth = 0;
    uint16_t retYear = 0;

    memcpy(responseMsg.data() + sizeof(completionCode) + hdrSize, &seconds,
           sizeof(seconds));
    memcpy(responseMsg.data() + sizeof(completionCode) + sizeof(seconds) +
               hdrSize,
           &minutes, sizeof(minutes));
    memcpy(responseMsg.data() + sizeof(completionCode) + sizeof(seconds) +
               sizeof(minutes) + hdrSize,
           &hours, sizeof(hours));
    memcpy(responseMsg.data() + sizeof(completionCode) + sizeof(seconds) +
               sizeof(minutes) + sizeof(hours) + hdrSize,
           &day, sizeof(day));
    memcpy(responseMsg.data() + sizeof(completionCode) + sizeof(seconds) +
               sizeof(minutes) + sizeof(hours) + sizeof(day) + hdrSize,
           &month, sizeof(month));
    memcpy(responseMsg.data() + sizeof(completionCode) + sizeof(seconds) +
               sizeof(minutes) + sizeof(hours) + sizeof(day) + sizeof(month) +
               hdrSize,
           &year, sizeof(year));

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = decode_get_date_time_resp(
        response, responseMsg.size() - hdrSize, &completionCode, &retSeconds,
        &retMinutes, &retHours, &retDay, &retMonth, &retYear);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(seconds, retSeconds);
    ASSERT_EQ(minutes, retMinutes);
    ASSERT_EQ(hours, retHours);
    ASSERT_EQ(day, retDay);
    ASSERT_EQ(month, retMonth);
    ASSERT_EQ(year, retYear);
}

TEST(GetBIOSTable, testGoodEncodeResponse)
{
    std::array<uint8_t,
               sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES + 4>
        responseMsg{};
    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    uint8_t completionCode = PLDM_SUCCESS;
    uint32_t nextTransferHandle = 32;
    uint8_t transferFlag = PLDM_START_AND_END;
    std::array<uint8_t, 4> tableData{1, 2, 3, 4};

    auto rc = encode_get_bios_table_resp(
        0, PLDM_SUCCESS, nextTransferHandle, transferFlag, tableData.data(),
        sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES + 4,
        response);
    ASSERT_EQ(rc, PLDM_SUCCESS);

    struct pldm_get_bios_table_resp* resp =
        reinterpret_cast<struct pldm_get_bios_table_resp*>(response->payload);

    ASSERT_EQ(completionCode, resp->completion_code);
    ASSERT_EQ(nextTransferHandle, resp->next_transfer_handle);
    ASSERT_EQ(transferFlag, resp->transfer_flag);
    ASSERT_EQ(0, memcmp(tableData.data(), resp->table_data, tableData.size()));
}

TEST(GetBIOSTable, testBadEncodeResponse)
{
    uint32_t nextTransferHandle = 32;
    uint8_t transferFlag = PLDM_START_AND_END;
    std::array<uint8_t, 4> tableData{1, 2, 3, 4};

    auto rc = encode_get_bios_table_resp(
        0, PLDM_SUCCESS, nextTransferHandle, transferFlag, tableData.data(),
        sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES + 4, nullptr);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(GetBIOSTable, testGoodDecodeRequest)
{
    const auto hdr_size = sizeof(pldm_msg_hdr);
    std::array<uint8_t, hdr_size + PLDM_GET_BIOS_TABLE_REQ_BYTES> requestMsg{};
    uint32_t transferHandle = 31;
    uint8_t transferOpFlag = PLDM_GET_FIRSTPART;
    uint8_t tableType = PLDM_BIOS_ATTR_TABLE;
    uint32_t retTransferHandle = 0;
    uint8_t retTransferOpFlag = 0;
    uint8_t retTableType = 0;

    auto req = reinterpret_cast<pldm_msg*>(requestMsg.data());
    struct pldm_get_bios_table_req* request =
        reinterpret_cast<struct pldm_get_bios_table_req*>(req->payload);

    request->transfer_handle = transferHandle;
    request->transfer_op_flag = transferOpFlag;
    request->table_type = tableType;

    auto rc = decode_get_bios_table_req(req, requestMsg.size() - hdr_size,
                                        &retTransferHandle, &retTransferOpFlag,
                                        &retTableType);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(transferHandle, retTransferHandle);
    ASSERT_EQ(transferOpFlag, retTransferOpFlag);
    ASSERT_EQ(tableType, retTableType);
}

TEST(GetBIOSTable, testBadDecodeRequest)
{
    const auto hdr_size = sizeof(pldm_msg_hdr);
    std::array<uint8_t, hdr_size + PLDM_GET_BIOS_TABLE_REQ_BYTES> requestMsg{};
    uint32_t transferHandle = 31;
    uint8_t transferOpFlag = PLDM_GET_FIRSTPART;
    uint8_t tableType = PLDM_BIOS_ATTR_TABLE;
    uint32_t retTransferHandle = 0;
    uint8_t retTransferOpFlag = 0;
    uint8_t retTableType = 0;

    auto req = reinterpret_cast<pldm_msg*>(requestMsg.data());
    struct pldm_get_bios_table_req* request =
        reinterpret_cast<struct pldm_get_bios_table_req*>(req->payload);

    request->transfer_handle = transferHandle;
    request->transfer_op_flag = transferOpFlag;
    request->table_type = tableType;

    auto rc = decode_get_bios_table_req(req, requestMsg.size() - hdr_size - 3,
                                        &retTransferHandle, &retTransferOpFlag,
                                        &retTableType);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(GetBIOSAttributeCurrentValueByHandle, testGoodDecodeRequest)
{
    uint32_t transferHandle = 45;
    uint8_t transferOpFlag = PLDM_GET_FIRSTPART;
    uint16_t attributehandle = 10;
    uint32_t retTransferHandle = 0;
    uint8_t retTransferOpFlag = 0;
    uint16_t retattributehandle = 0;
    std::array<uint8_t, hdrSize + sizeof(transferHandle) +
                            sizeof(transferOpFlag) + sizeof(attributehandle)>
        requestMsg{};

    auto req = reinterpret_cast<pldm_msg*>(requestMsg.data());
    struct pldm_attribute_current_value_by_handle_req* request =
        reinterpret_cast<struct pldm_attribute_current_value_by_handle_req*>(
            req->payload);

    request->transfer_handle = transferHandle;
    request->transfer_op_flag = transferOpFlag;
    request->attribute_handle = attributehandle;

    auto rc = decode_get_bios_attribute_value_by_handle_req(
        req, requestMsg.size() - hdrSize, &retTransferHandle,
        &retTransferOpFlag, &retattributehandle);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(transferHandle, retTransferHandle);
    ASSERT_EQ(transferOpFlag, retTransferOpFlag);
    ASSERT_EQ(attributehandle, retattributehandle);
}

TEST(GetBIOSAttributeCurrentValueByHandle, testBadDecodeRequest)
{

    uint32_t transferHandle = 31;
    uint8_t transferOpFlag = PLDM_GET_FIRSTPART;
    uint8_t attribute_handle = 0;
    uint32_t retTransferHandle = 0;
    uint8_t retTransferOpFlag = 0;
    uint8_t retattribute_handle = 0;
    std::array<uint8_t, hdrSize + sizeof(transferHandle) +
                            sizeof(transferOpFlag) + sizeof(attribute_handle)>
        requestMsg{};

    auto req = reinterpret_cast<pldm_msg*>(requestMsg.data());
    struct pldm_attribute_current_value_by_handle_req* request =
        reinterpret_cast<struct pldm_attribute_current_value_by_handle_req*>(
            req->payload);

    request->transfer_handle = transferHandle;
    request->transfer_op_flag = transferOpFlag;
    request->attribute_handle = attribute_handle;

    auto rc = decode_get_bios_table_req(req, requestMsg.size() - hdrSize - 3,
                                        &retTransferHandle, &retTransferOpFlag,
                                        &retattribute_handle);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(GetBIOSAttributeCurrentValueByHandle, testGoodEncodeResponse)
{

    uint8_t instance_id = 10;
    uint8_t completionCode = PLDM_SUCCESS;
    uint32_t nextTransferHandle = 32;
    uint8_t transferFlag = PLDM_START_AND_END;
    uint8_t attribute_data = 44;
    std::array<uint8_t, hdrSize + sizeof(completionCode) +
                            sizeof(nextTransferHandle) + sizeof(transferFlag) +
                            sizeof(attribute_data)>
        responseMsg{};
    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = encode_get_bios_current_value_by_handle_resp(
        instance_id, completionCode, nextTransferHandle, transferFlag,
        &attribute_data, sizeof(attribute_data), response);

    ASSERT_EQ(rc, PLDM_SUCCESS);

    struct pldm_attribute_current_value_by_handle_resp* resp =
        reinterpret_cast<struct pldm_attribute_current_value_by_handle_resp*>(
            response->payload);

    ASSERT_EQ(completionCode, resp->completion_code);
    ASSERT_EQ(nextTransferHandle, resp->next_transfer_handle);
    ASSERT_EQ(transferFlag, resp->transfer_flag);
    ASSERT_EQ(0, memcmp(&attribute_data, resp->attribute_data,
                        sizeof(attribute_data)));
}

TEST(GetBIOSAttributeCurrentValueByHandle, testBadEncodeResponse)
{
    uint32_t nextTransferHandle = 32;
    uint8_t transferFlag = PLDM_START_AND_END;
    uint8_t attribute_data = 44;

    auto rc = encode_get_bios_current_value_by_handle_resp(
        0, PLDM_SUCCESS, nextTransferHandle, transferFlag, &attribute_data,
        sizeof(pldm_msg_hdr) + PLDM_GET_ATTR_VAL_BY_HANDLE_MIN_VAL, nullptr);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}
