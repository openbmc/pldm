#include <string.h>

#include <array>

#include "libpldm/base.h"

#include <gtest/gtest.h>

TEST(GetPLDMCommands, testEncodeRequest)
{
    uint8_t pldmType = 0x05;
    pldm_version_t version{0xFF, 0xFF, 0xFF, 0xFF};
    std::array<uint8_t, PLDM_GET_COMMANDS_REQ_BYTES> requestMsg{};
    pldm_msg_t request{};
    request.body.payload = requestMsg.data();
    request.body.payload_length = requestMsg.size();

    auto rc = encode_get_commands_req(0, pldmType, version, &request);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(request.body.payload, &pldmType, sizeof(pldmType)));
    ASSERT_EQ(0, memcmp(request.body.payload + sizeof(pldmType), &version,
                        sizeof(version)));
}

TEST(GetPLDMCommands, testDecodeRequest)
{
    uint8_t pldmType = 0x05;
    pldm_version_t version{0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t pldmTypeOut{};
    pldm_version_t versionOut{0xFF, 0xFF, 0xFF, 0xFF};
    std::array<uint8_t, PLDM_GET_COMMANDS_REQ_BYTES> requestMsg{};
    pldm_msg_payload_t request{};
    request.payload = requestMsg.data();
    request.payload_length = requestMsg.size();

    memcpy(request.payload, &pldmType, sizeof(pldmType));
    memcpy(request.payload + sizeof(pldmType), &version, sizeof(version));
    auto rc = decode_get_commands_req(&request, &pldmTypeOut, &versionOut);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(pldmTypeOut, pldmType);
    ASSERT_EQ(0, memcmp(&versionOut, &version, sizeof(version)));
}

TEST(GetPLDMCommands, testEncodeResponse)
{
    uint8_t completionCode = 0;
    std::array<uint8_t, PLDM_GET_COMMANDS_RESP_BYTES> responseMsg{};
    pldm_msg_t response{};
    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();
    std::array<uint8_t, PLDM_MAX_CMDS_PER_TYPE / 8> commands{};
    commands[0] = 1;
    commands[1] = 2;
    commands[2] = 3;

    auto rc = encode_get_commands_resp(0, commands.data(), &response);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completionCode, response.body.payload[0]);
    ASSERT_EQ(1, response.body.payload[1]);
    ASSERT_EQ(2, response.body.payload[2]);
    ASSERT_EQ(3, response.body.payload[3]);
}

TEST(GetPLDMTypes, testEncodeResponse)
{
    uint8_t completionCode = 0;
    std::array<uint8_t, PLDM_GET_TYPES_RESP_BYTES> responseMsg{};
    pldm_msg_t response{};
    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();
    std::array<uint8_t, PLDM_MAX_TYPES / 8> types{};
    types[0] = 1;
    types[1] = 2;
    types[2] = 3;

    auto rc = encode_get_types_resp(0, types.data(), &response);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completionCode, response.body.payload[0]);
    ASSERT_EQ(1, response.body.payload[1]);
    ASSERT_EQ(2, response.body.payload[2]);
    ASSERT_EQ(3, response.body.payload[3]);
}
