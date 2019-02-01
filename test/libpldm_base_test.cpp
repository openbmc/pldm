#include <string.h>

#include <array>

#include "libpldm/base.h"

#include <gtest/gtest.h>

TEST(GetPLDMCommands, testEncodeRequest)
{
    pldm_msg_t request{};
    uint8_t pldmType = 0x05;
    pldm_version_t version{0xFF, 0xFF, 0xFF, 0xFF};
    std::array<uint8_t, sizeof(pldmType) + sizeof(version)> requestPayload{};
    request.payload = requestPayload.data();

    auto rc = encode_get_commands_req(0, pldmType, version, &request);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(request.payload, &pldmType, sizeof(pldmType)));
    ASSERT_EQ(0, memcmp(request.payload + sizeof(pldmType), &version,
                        sizeof(version)));
}

TEST(GetPLDMCommands, testDecodeRequest)
{
    std::array<uint8_t, sizeof(pldm_version_t) + sizeof(uint8_t)>
        requestPayload{};
    pldm_msg_t request{};
    request.payload = requestPayload.data();
    uint8_t pldmType = 0x05;
    pldm_version_t version{0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t pldmTypeOut{};
    pldm_version_t versionOut{0xFF, 0xFF, 0xFF, 0xFF};

    memcpy(request.payload, &pldmType, sizeof(pldmType));
    memcpy(request.payload + sizeof(pldmType), &version, sizeof(version));
    auto rc = decode_get_commands_req(&request, &pldmTypeOut, &versionOut);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(pldmTypeOut, pldmType);
    ASSERT_EQ(0, memcmp(&versionOut, &version, sizeof(version)));
}

TEST(GetPLDMCommands, testEncodeResponse)
{
    pldm_msg_t response{};
    uint8_t completionCode = 0;
    std::array<uint8_t, (PLDM_MAX_CMDS_PER_TYPE / 8) + sizeof(completionCode)>
        responsePayload{};
    response.payload = responsePayload.data();
    responsePayload[1] = 1;
    responsePayload[2] = 2;
    responsePayload[3] = 3;

    auto rc =
        encode_get_commands_resp(0, responsePayload.data() + 1, &response);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completionCode, response.payload[0]);
    ASSERT_EQ(1, response.payload[1]);
    ASSERT_EQ(2, response.payload[2]);
    ASSERT_EQ(3, response.payload[3]);
}

TEST(GetPLDMTypes, testEncodeResponse)
{
    pldm_msg_t response{};
    uint8_t completionCode = 0;
    std::array<uint8_t, (PLDM_MAX_TYPES / 8) + sizeof(completionCode)>
        responsePayload{};
    response.payload = responsePayload.data();
    responsePayload[1] = 1;
    responsePayload[2] = 2;
    responsePayload[3] = 3;

    auto rc = encode_get_types_resp(0, responsePayload.data() + 1, &response);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(0, response.payload[0]);
    ASSERT_EQ(1, response.payload[1]);
    ASSERT_EQ(2, response.payload[2]);
    ASSERT_EQ(3, response.payload[3]);
}
