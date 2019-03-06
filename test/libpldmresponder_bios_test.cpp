
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
    /* Fri Oct 13 18:31:43 2028 */
    uint64_t epochTime = 1855092703;

    uint8_t seconds = 0;
    uint8_t minutes = 0;
    uint8_t hours = 0;
    uint8_t day = 0;
    uint8_t month = 0;
    uint16_t year = 0;

    epochToBCDTime(epochTime, seconds, minutes, hours, day, month, year);

    ASSERT_EQ(0x43, seconds);
    ASSERT_EQ(0x31, minutes);
    ASSERT_EQ(0x18, hours);
    ASSERT_EQ(0x13, day);
    ASSERT_EQ(0x10, month);
    ASSERT_EQ(0x2028, year);
}
