#include <string.h>

#include <array>

#include "libpldm/base.h"
#include "libpldm/bios.h"

#include <gtest/gtest.h>

constexpr auto hdr_size = sizeof(pldm_msg_hdr);

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
    std::array<uint8_t, hdr_size + PLDM_GET_DATE_TIME_RESP_BYTES> responseMsg{};

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

    memcpy(responseMsg.data() + sizeof(completionCode) + hdr_size, &seconds,
           sizeof(seconds));
    memcpy(responseMsg.data() + sizeof(completionCode) + sizeof(seconds) +
               hdr_size,
           &minutes, sizeof(minutes));
    memcpy(responseMsg.data() + sizeof(completionCode) + sizeof(seconds) +
               sizeof(minutes) + hdr_size,
           &hours, sizeof(hours));
    memcpy(responseMsg.data() + sizeof(completionCode) + sizeof(seconds) +
               sizeof(minutes) + sizeof(hours) + hdr_size,
           &day, sizeof(day));
    memcpy(responseMsg.data() + sizeof(completionCode) + sizeof(seconds) +
               sizeof(minutes) + sizeof(hours) + sizeof(day) + hdr_size,
           &month, sizeof(month));
    memcpy(responseMsg.data() + sizeof(completionCode) + sizeof(seconds) +
               sizeof(minutes) + sizeof(hours) + sizeof(day) + sizeof(month) +
               hdr_size,
           &year, sizeof(year));

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = decode_get_date_time_resp(
        response, responseMsg.size() - hdr_size, &completionCode, &retSeconds,
        &retMinutes, &retHours, &retDay, &retMonth, &retYear);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(seconds, retSeconds);
    ASSERT_EQ(minutes, retMinutes);
    ASSERT_EQ(hours, retHours);
    ASSERT_EQ(day, retDay);
    ASSERT_EQ(month, retMonth);
    ASSERT_EQ(year, retYear);
}
