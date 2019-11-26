#include <cstring>
#include <vector>

#include "libpldm/utils.h"

#include <gtest/gtest.h>

TEST(Crc32, CheckSumTest)
{
    const char* password = "123456789";
    auto checksum = crc32(password, 9);
    EXPECT_EQ(checksum, 0xcbf43926);
}

TEST(Crc8, CheckSumTest)
{
    const char* data = "123456789";
    auto checksum = crc8(data, 9);
    EXPECT_EQ(checksum, 0xf4);
}

TEST(Ver2string, Ver2string)
{
    ver32_t version{0xf3, 0xf7, 0x10, 0x61};
    const char* vstr = "3.7.10a";
    char buffer[1024];
    auto rc = ver2str(&version, buffer, sizeof(buffer));
    EXPECT_EQ(rc, std::strlen(vstr));
    EXPECT_STREQ(vstr, buffer);

    version = {0x10, 0x01, 0xf7, 0x00};
    vstr = "10.01.7";
    rc = ver2str(&version, buffer, sizeof(buffer));
    EXPECT_EQ(rc, std::strlen(vstr));
    EXPECT_STREQ(vstr, buffer);

    version = {0xf3, 0xf1, 0xff, 0x00};
    vstr = "3.1";
    rc = ver2str(&version, buffer, sizeof(buffer));
    EXPECT_EQ(rc, std::strlen(vstr));
    EXPECT_STREQ(vstr, buffer);

    version = {0xf1, 0xf0, 0xff, 0x61};
    vstr = "1.0a";
    rc = ver2str(&version, buffer, sizeof(buffer));
    EXPECT_EQ(rc, std::strlen(vstr));
    EXPECT_STREQ(vstr, buffer);

    rc = ver2str(&version, buffer, 3);
    EXPECT_EQ(rc, 2);
    EXPECT_STREQ("1.", buffer);

    rc = ver2str(&version, buffer, 1);
    EXPECT_EQ(rc, 0);

    rc = ver2str(&version, buffer, 0);
    EXPECT_EQ(rc, -1);
}

TEST(BcdConversion, BcdCoversion)
{
    EXPECT_EQ(12, bcd2dec8(0x12));
    EXPECT_EQ(99, bcd2dec8(0x99));
    EXPECT_EQ(1234, bcd2dec16(0x1234));
    EXPECT_EQ(9999, bcd2dec16(0x9999));
    EXPECT_EQ(12345678, bcd2dec32(0x12345678));
    EXPECT_EQ(99999999, bcd2dec32(0x99999999));

    EXPECT_EQ(0x12, dec2bcd8(12));
    EXPECT_EQ(0x99, dec2bcd8(99));
    EXPECT_EQ(0x1234, dec2bcd16(1234));
    EXPECT_EQ(0x9999, dec2bcd16(9999));
    EXPECT_EQ(0x12345678, dec2bcd32(12345678));
    EXPECT_EQ(0x99999999, dec2bcd32(99999999));
}