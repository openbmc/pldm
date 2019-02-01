#include "libpldm/base.h"
#include "libpldmresponder/base.hpp"
#include <gtest/gtest.h>
#include <string.h>
#include <array>

using namespace pldm::responder;

TEST(GetPLDMTypes, testGoodRequest)
{
    auto response = getPLDMTypes(nullptr, 0);
    // Only base type supported at the moment
    ASSERT_EQ(response[PLDM_RESPONSE_HEADER_LEN_BYTES], 1);
    ASSERT_EQ(response[PLDM_RESPONSE_HEADER_LEN_BYTES + 1], 0);
    ASSERT_EQ(
        response[PLDM_RESPONSE_HEADER_LEN_BYTES +
                 PLDM_GET_TYPES_RESP_DATA_BYTES - 1],
        0);
}

TEST(GetPLDMCommands, testGoodRequest)
{
    // Only base type supported at the moment, and commands -
    // GetPLDMTypes, GetPLDMCommands
    std::array<uint8_t, 5> request{};
    auto response = getPLDMCommands(request.data(), request.size());
    ASSERT_EQ(response[PLDM_RESPONSE_HEADER_LEN_BYTES], 48); // 48 = 0b110000
    ASSERT_EQ(response[PLDM_RESPONSE_HEADER_LEN_BYTES + 1], 0);
    ASSERT_EQ(
        response[PLDM_RESPONSE_HEADER_LEN_BYTES +
                 PLDM_GET_TYPES_RESP_DATA_BYTES - 1],
        0);
}
