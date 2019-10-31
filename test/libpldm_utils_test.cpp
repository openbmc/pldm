#include <vector>

#include "libpldm/utils.h"

#include <gtest/gtest.h>

TEST(Crc32, CheckSumTest)
{
    const char* password = "123456789";
    auto checksum = crc32(password, 9);
    EXPECT_EQ(checksum, 0xcbf43926);
}