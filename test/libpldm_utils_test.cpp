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

TEST(Ver2string, Ver2string)
{
    ver32_t version{0xf3, 0xf7, 0x10, 0x61};
    const char* vstr = "3.7.10a";
    char buffer[1024];
    auto rc = ver2str(&version, buffer, sizeof(buffer));
    std::cerr << buffer << std::endl;
    EXPECT_EQ(rc, std::strlen(vstr));
    EXPECT_EQ(std::strcmp(vstr, buffer), 0);

    version = {0x10, 0x01, 0xf7, 0x00};
    vstr = "10.01.7";
    rc = ver2str(&version, buffer, sizeof(buffer));
    std::cerr << buffer << std::endl;
    EXPECT_EQ(rc, std::strlen(vstr));
    EXPECT_EQ(std::strcmp(vstr, buffer), 0);

    version = {0xf3, 0xf1, 0xff, 0x00};
    vstr = "3.1";
    rc = ver2str(&version, buffer, sizeof(buffer));
    std::cerr << buffer << std::endl;
    EXPECT_EQ(rc, std::strlen(vstr));
    EXPECT_EQ(std::strcmp(vstr, buffer), 0);

    version = {0xf1, 0xf0, 0xff, 0x61};
    vstr = "1.0a";
    rc = ver2str(&version, buffer, sizeof(buffer));
    std::cerr << buffer << std::endl;
    EXPECT_EQ(rc, std::strlen(vstr));
    EXPECT_EQ(std::strcmp(vstr, buffer), 0);

    rc = ver2str(&version, buffer, 3);
    std::cerr << rc << std::endl;
    std::cerr << buffer << std::endl;
    EXPECT_EQ(rc, 2);
    EXPECT_EQ(std::strcmp("1.", buffer), 0);
}