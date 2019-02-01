#include "libpldmresponder/base.hpp"

#include <string.h>

#include <array>

#include "libpldm/base.h"

#include <gtest/gtest.h>

using namespace pldm::responder;

TEST(GetPLDMTypes, testGoodRequest)
{
    pldm_msg_payload_t request{};
    pldm_msg_t response{};
    std::array<uint8_t, PLDM_GET_TYPES_RESP_BYTES> responseMsg{};
    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();
    getPLDMTypes(&request, &response);
    // Only base type supported at the moment
    ASSERT_EQ(response.body.payload[0], 0);
    ASSERT_EQ(response.body.payload[1], 1);
    ASSERT_EQ(response.body.payload[2], 0);
}

TEST(GetPLDMCommands, testGoodRequest)
{
    // Only base type supported at the moment, and commands -
    // GetPLDMTypes, GetPLDMCommands
    pldm_msg_t response{};
    std::array<uint8_t, PLDM_GET_COMMANDS_RESP_BYTES> responseMsg{};
    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();
    pldm_msg_payload_t request{};
    std::array<uint8_t, 5> requestPayload{};
    request.payload = requestPayload.data();
    request.payload_length = requestPayload.size();
    getPLDMCommands(&request, &response);
    ASSERT_EQ(response.body.payload[0], 0);
    ASSERT_EQ(response.body.payload[1], 48); // 48 = 0b110000
    ASSERT_EQ(response.body.payload[2], 0);
}
