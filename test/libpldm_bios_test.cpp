#include <string.h>

#include <array>

#include "libpldm/base.h"
#include "libpldm/bios.h"

#include <gtest/gtest.h>

TEST(GetDateTime, testEncodeResponse)
{
    uint8_t completionCode = 0;
    uint8_t seconds = 50;
    uint8_t minutes = 20;
    uint8_t hours = 5;
    uint8_t day = 23;
    uint8_t month = 11;
    uint16_t year = 2019;

    std::array<uint8_t, PLDM_GET_DATE_TIME_RESP_BYTES> responseMsg{};
    pldm_msg response{};

    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();

    auto rc = encode_get_date_time_resp(0, PLDM_SUCCESS, &seconds, &minutes,
                                        &hours, &day, &month, &year, &response);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completionCode, response.body.payload[0]);

    ASSERT_EQ(0,
              memcmp(response.body.payload + sizeof(response.body.payload[0]),
                     &seconds, sizeof(seconds)));
    ASSERT_EQ(0, memcmp(response.body.payload +
                            sizeof(response.body.payload[0]) + sizeof(seconds),
                        &minutes, sizeof(minutes)));
    ASSERT_EQ(0,
              memcmp(response.body.payload + sizeof(response.body.payload[0]) +
                         sizeof(seconds) + sizeof(minutes),
                     &hours, sizeof(hours)));
    ASSERT_EQ(0,
              memcmp(response.body.payload + sizeof(response.body.payload[0]) +
                         sizeof(seconds) + sizeof(minutes) + sizeof(hours),
                     &day, sizeof(day)));
    ASSERT_EQ(0, memcmp(response.body.payload +
                            sizeof(response.body.payload[0]) + sizeof(seconds) +
                            sizeof(minutes) + sizeof(hours) + sizeof(day),
                        &month, sizeof(month)));
    ASSERT_EQ(0,
              memcmp(response.body.payload + sizeof(response.body.payload[0]) +
                         sizeof(seconds) + sizeof(minutes) + sizeof(hours) +
                         sizeof(day) + sizeof(month),
                     &year, sizeof(year)));
}

TEST(GetDateTime, testDecodeResponse)
{
    std::array<uint8_t, PLDM_GET_DATE_TIME_RESP_BYTES> responseMsg{};
    pldm_msg_payload response{};
    response.payload = responseMsg.data();
    response.payload_length = responseMsg.size();

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

    memcpy(response.payload + sizeof(completionCode), &seconds,
           sizeof(seconds));
    memcpy(response.payload + sizeof(completionCode) + sizeof(seconds),
           &minutes, sizeof(minutes));
    memcpy(response.payload + sizeof(completionCode) + sizeof(seconds) +
               sizeof(minutes),
           &hours, sizeof(hours));
    memcpy(response.payload + sizeof(completionCode) + sizeof(seconds) +
               sizeof(minutes) + sizeof(hours),
           &day, sizeof(day));
    memcpy(response.payload + sizeof(completionCode) + sizeof(seconds) +
               sizeof(minutes) + sizeof(hours) + sizeof(day),
           &month, sizeof(month));
    memcpy(response.payload + sizeof(completionCode) + sizeof(seconds) +
               sizeof(minutes) + sizeof(hours) + sizeof(day) + sizeof(month),
           &year, sizeof(year));

    auto rc = decode_get_date_time_resp(&response, &completionCode, &retSeconds,
                                        &retMinutes, &retHours, &retDay,
                                        &retMonth, &retYear);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(seconds, retSeconds);
    ASSERT_EQ(minutes, retMinutes);
    ASSERT_EQ(hours, retHours);
    ASSERT_EQ(day, retDay);
    ASSERT_EQ(month, retMonth);
    ASSERT_EQ(year, retYear);
}
