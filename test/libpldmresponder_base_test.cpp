#include "libpldmresponder/base.hpp"

#include <string.h>

#include <array>

#include "libpldm/base.h"

#include <gtest/gtest.h>

using namespace pldm::responder;

TEST(GetPLDMTypes, testGoodRequest)
{
    pldm_msg_t response{};
    std::array<uint8_t, PLDM_GET_TYPES_RESP_DATA_BYTES> responsePayload{};
    response.payload = responsePayload.data();
    getPLDMTypes(nullptr, 0, &response);
    // Only base type supported at the moment
    ASSERT_EQ(response.payload[0], 0);
    ASSERT_EQ(response.payload[1], 1);
    ASSERT_EQ(response.payload[2], 0);
}

TEST(GetPLDMCommands, testGoodRequest)
{
    // Only base type supported at the moment, and commands -
    // GetPLDMTypes, GetPLDMCommands
    pldm_msg_t request{};
    pldm_msg_t response{};
    std::array<uint8_t, PLDM_GET_COMMANDS_RESP_DATA_BYTES> responsePayload{};
    response.payload = responsePayload.data();
    std::array<uint8_t, 5> requestPayload{};
    request.payload = requestPayload.data();
    getPLDMCommands(&request, requestPayload.size(), &response);
    ASSERT_EQ(response.payload[0], 0);
    ASSERT_EQ(response.payload[1], 48); // 48 = 0b110000
    ASSERT_EQ(response.payload[2], 0);
}
