#include "libpldmresponder/base.hpp"

#include <string.h>

#include <array>

#include "libpldm/base.h"

#include <gtest/gtest.h>

using namespace pldm::responder;

TEST(GetPLDMTypes, testGoodRequest)
{
    pldm_msg_payload request{};
    pldm_msg response{};
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
    pldm_msg response{};
    std::array<uint8_t, PLDM_GET_COMMANDS_RESP_BYTES> responseMsg{};
    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();
    pldm_msg_payload request{};
    std::array<uint8_t, 5> requestPayload{};
    request.payload = requestPayload.data();
    request.payload_length = requestPayload.size();
    getPLDMCommands(&request, &response);
    ASSERT_EQ(response.body.payload[0], 0);
    ASSERT_EQ(response.body.payload[1], 48); // 48 = 0b110000
    ASSERT_EQ(response.body.payload[2], 0);
}

TEST(GetPLDMCommands, testBadRequest)
{
    pldm_msg response{};
    std::array<uint8_t, PLDM_GET_COMMANDS_RESP_BYTES> responseMsg{};
    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();
    pldm_msg_payload request{};
    std::array<uint8_t, 5> requestPayload{};

    request.payload = requestPayload.data();
    request.payload[0] = 0xFF;
    request.payload_length = requestPayload.size();
    getPLDMCommands(&request, &response);
    ASSERT_EQ(response.body.payload[0], PLDM_ERROR_INVALID_PLDM_TYPE);
}

TEST(GetPLDMVersion, testGoodRequest)
{
    pldm_msg response{};
    std::array<uint8_t, PLDM_GET_VERSION_RESP_BYTES> responseMsg{};
    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();
    pldm_msg request{};
    std::array<uint8_t, PLDM_GET_VERSION_REQ_BYTES> requestPayload{};
    request.body.payload = requestPayload.data();
    request.body.payload_length = requestPayload.size();

    uint8_t pldmType = PLDM_BASE;
    uint32_t transferHandle = 0x0;
    uint8_t flag = PLDM_GET_FIRSTPART;
    uint8_t retFlag = PLDM_START_AND_END;
    ver32_t version = {0xF1, 0xF0, 0xF0, 0x00};

    auto rc =
        encode_get_version_req(0, transferHandle, flag, pldmType, &request);

    ASSERT_EQ(0, rc);

    getPLDMVersion(&(request.body), &response);

    ASSERT_EQ(response.body.payload[0], 0);
    ASSERT_EQ(0,
              memcmp(response.body.payload + sizeof(response.body.payload[0]),
                     &transferHandle, sizeof(transferHandle)));
    ASSERT_EQ(0,
              memcmp(response.body.payload + sizeof(response.body.payload[0]) +
                         sizeof(transferHandle),
                     &retFlag, sizeof(flag)));
    ASSERT_EQ(0,
              memcmp(response.body.payload + sizeof(response.body.payload[0]) +
                         sizeof(transferHandle) + sizeof(flag),
                     &version, sizeof(version)));
}
