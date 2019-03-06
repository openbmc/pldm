
#include "libpldmresponder/utils.hpp"

#include <string.h>

#include <array>

#include "libpldm/base.h"
#include "libpldm/bios.h"

#include <gtest/gtest.h>

using namespace pldm::responder;

TEST(formatTime, testTime)
{
    std::string time = "Tue Nov 26 05:09:42 2047";
    uint8_t seconds = 0;
    uint8_t minutes = 0;
    uint8_t hours = 0;
    uint8_t day = 0;
    uint8_t month = 0;
    uint16_t year = 0;

    formatTime(time, seconds, minutes, hours, day, month, year);

    ASSERT_EQ(66, seconds);
    ASSERT_EQ(9, minutes);
    ASSERT_EQ(5, hours);
    ASSERT_EQ(38, day);
    ASSERT_EQ(17, month);
    ASSERT_EQ(8263, year);
}
