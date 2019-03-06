
#include "libpldmresponder/bios.hpp"

#include <string.h>

#include <array>
#include <ctime>

#include "libpldm/base.h"
#include "libpldm/bios.h"

#include <gtest/gtest.h>

using namespace pldm::responder;

TEST(formatTime, testTime)
{
    struct tm timeInfo;
    timeInfo.tm_sec = 42;
    timeInfo.tm_min = 9;
    timeInfo.tm_hour = 5;
    timeInfo.tm_mday = 26;
    timeInfo.tm_year = 2047;
    timeInfo.tm_mon = 11;

    uint8_t seconds = 0;
    uint8_t minutes = 0;
    uint8_t hours = 0;
    uint8_t day = 0;
    uint8_t month = 0;
    uint16_t year = 0;

    formatTime(&timeInfo, seconds, minutes, hours, day, month, year);

    ASSERT_EQ(66, seconds);
    ASSERT_EQ(9, minutes);
    ASSERT_EQ(5, hours);
    ASSERT_EQ(38, day);
    ASSERT_EQ(17, month);
    ASSERT_EQ(8263, year);
}
