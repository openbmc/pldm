#include <boost/crc.hpp>
#include <vector>

#include "libpldm/utils.h"

#include <gtest/gtest.h>

TEST(Crc32, CheckSumTest)
{
    std::vector<uint8_t> table = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    boost::crc_32_type result;
    result.process_bytes(table.data(), table.size());
    auto checksum = crc32(table.data(), table.size());
    EXPECT_EQ(checksum, result.checksum());
}