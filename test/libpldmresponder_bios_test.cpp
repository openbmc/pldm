
#include "libpldmresponder/bios.hpp"

#include <string.h>

#include <array>
#include <ctime>

#include "libpldm/base.h"
#include "libpldm/bios.h"

#include <gtest/gtest.h>

using namespace pldm::responder;

TEST(epochToBCDTime, testTime)
{
    /* Fri Apr 12 04:51:45 2019 */
    uint64_t epochTime = 1555062705;

    uint8_t seconds = 0;
    uint8_t minutes = 0;
    uint8_t hours = 0;
    uint8_t day = 0;
    uint8_t month = 0;
    uint16_t year = 0;

    epochToBCDTime(epochTime, seconds, minutes, hours, day, month, year);

    ASSERT_EQ(0x45, seconds);
    ASSERT_EQ(0x51, minutes);
    ASSERT_EQ(0x4, hours);
    ASSERT_EQ(0x12, day);
    ASSERT_EQ(0x4, month);
    ASSERT_EQ(0x2019, year);
}
