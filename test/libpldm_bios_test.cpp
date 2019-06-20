#include <string.h>

#include <array>

#include "libpldm/base.h"
#include "libpldm/bios.h"

#include <gtest/gtest.h>

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
    std::array<uint8_t, PLDM_GET_DATE_TIME_RESP_BYTES> responseMsg{};

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

    memcpy(responseMsg.data() + sizeof(completionCode), &seconds,
           sizeof(seconds));
    memcpy(responseMsg.data() + sizeof(completionCode) + sizeof(seconds),
           &minutes, sizeof(minutes));
    memcpy(responseMsg.data() + sizeof(completionCode) + sizeof(seconds) +
               sizeof(minutes),
           &hours, sizeof(hours));
    memcpy(responseMsg.data() + sizeof(completionCode) + sizeof(seconds) +
               sizeof(minutes) + sizeof(hours),
           &day, sizeof(day));
    memcpy(responseMsg.data() + sizeof(completionCode) + sizeof(seconds) +
               sizeof(minutes) + sizeof(hours) + sizeof(day),
           &month, sizeof(month));
    memcpy(responseMsg.data() + sizeof(completionCode) + sizeof(seconds) +
               sizeof(minutes) + sizeof(hours) + sizeof(day) + sizeof(month),
           &year, sizeof(year));

    auto rc = decode_get_date_time_resp(
        responseMsg.data(), responseMsg.size(), &completionCode, &retSeconds,
        &retMinutes, &retHours, &retDay, &retMonth, &retYear);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(seconds, retSeconds);
    ASSERT_EQ(minutes, retMinutes);
    ASSERT_EQ(hours, retHours);
    ASSERT_EQ(day, retDay);
    ASSERT_EQ(month, retMonth);
    ASSERT_EQ(year, retYear);
}

TEST(GetBIOSTable, testEncodeResponse)
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
        PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES + 4, response);
    ASSERT_EQ(rc, PLDM_SUCCESS);

    struct pldm_get_bios_table_resp* resp =
        reinterpret_cast<struct pldm_get_bios_table_resp*>(response->payload);

    ASSERT_EQ(completionCode, resp->completion_code);
    ASSERT_EQ(nextTransferHandle, resp->next_transfer_handle);
    ASSERT_EQ(transferFlag, resp->transfer_flag);
    ASSERT_EQ(0, memcmp(tableData.data(), resp->table_data, tableData.size()));
}

TEST(GetBIOSTable, testDecodeRequest)
{
    std::array<uint8_t, PLDM_GET_BIOS_TABLE_REQ_BYTES> requestMsg{};
    uint32_t transferHandle = 31;
    uint8_t transferOpFlag = PLDM_GET_FIRSTPART;
    uint8_t tableType = PLDM_BIOS_ATTR_TABLE;
    uint32_t retTransferHandle = 0;
    uint8_t retTransferOpFlag = 0;
    uint8_t retTableType = 0;

    struct pldm_get_bios_table_req* request =
        reinterpret_cast<struct pldm_get_bios_table_req*>(requestMsg.data());

    request->transfer_handle = transferHandle;
    request->transfer_op_flag = transferOpFlag;
    request->table_type = tableType;

    auto rc = decode_get_bios_table_req(requestMsg.data(), requestMsg.size(),
                                        &retTransferHandle, &retTransferOpFlag,
                                        &retTableType);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(transferHandle, retTransferHandle);
    ASSERT_EQ(transferOpFlag, retTransferOpFlag);
    ASSERT_EQ(tableType, retTableType);
}
