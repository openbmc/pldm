#include <string.h>

#include <array>

#include "libpldm/base.h"

#include <gtest/gtest.h>

TEST(GetPLDMCommands, testEncodeRequest)
{
    std::array<uint8_t, PLDM_REQUEST_HEADER_LEN_BYTES +
                            // PLDM version
                            sizeof(pldm_version_t) +
                            // PLDM Type
                            sizeof(uint8_t)>
        buffer{};
    uint8_t pldmType = 0x05;
    pldm_version_t version{0xFF, 0xFF, 0xFF, 0xFF};

    auto rc = encode_get_commands_req(0, pldmType, version, buffer.data());
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(buffer.data() + PLDM_REQUEST_HEADER_LEN_BYTES,
                        &pldmType, sizeof(pldmType)));
    ASSERT_EQ(0, memcmp(buffer.data() + PLDM_REQUEST_HEADER_LEN_BYTES +
                            sizeof(pldmType),
                        &version, sizeof(version)));
}

TEST(GetPLDMCommands, testDecodeRequest)
{
    std::array<uint8_t, PLDM_REQUEST_HEADER_LEN_BYTES +
                            // PLDM version
                            sizeof(pldm_version_t) +
                            // PLDM Type
                            sizeof(uint8_t)>
        buffer{};
    uint8_t pldmType = 0x05;
    pldm_version_t version{0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t pldmTypeOut{};
    pldm_version_t versionOut{0xFF, 0xFF, 0xFF, 0xFF};

    memcpy(buffer.data() + PLDM_REQUEST_HEADER_LEN_BYTES,
           &pldmType, sizeof(pldmType));
    memcpy(buffer.data() + PLDM_REQUEST_HEADER_LEN_BYTES + sizeof(pldmType),
           &version, sizeof(version));
    auto rc = decode_get_commands_req(buffer.data(), &pldmTypeOut, &versionOut);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(pldmTypeOut, pldmType);
    ASSERT_EQ(0, memcmp(&versionOut, &version, sizeof(version)));
}

TEST(GetPLDMCommands, testEncodeResponse)
{
    std::array<uint8_t, PLDM_RESPONSE_HEADER_LEN_BYTES + PLDM_MAX_CMDS_PER_TYPE>
        buffer{};
    std::array<uint8_t, PLDM_MAX_CMDS_PER_TYPE> commands{};
    commands[0] = 1;
    commands[1] = 2;
    commands[2] = 3;

    auto rc = encode_get_commands_resp(0, 0, commands.data(), buffer.data());
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(1, buffer[PLDM_RESPONSE_HEADER_LEN_BYTES]);
    ASSERT_EQ(2, buffer[PLDM_RESPONSE_HEADER_LEN_BYTES + 1]);
    ASSERT_EQ(3, buffer[PLDM_RESPONSE_HEADER_LEN_BYTES + 2]);
}

TEST(GetPLDMTypes, testEncodeResponse)
{
    std::array<uint8_t, PLDM_RESPONSE_HEADER_LEN_BYTES + PLDM_MAX_TYPES>
        buffer{};
    std::array<uint8_t, PLDM_MAX_TYPES> types{};
    types[0] = 1;
    types[1] = 2;
    types[2] = 3;

    auto rc = encode_get_types_resp(0, 0, types.data(), buffer.data());
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(1, buffer[PLDM_RESPONSE_HEADER_LEN_BYTES]);
    ASSERT_EQ(2, buffer[PLDM_RESPONSE_HEADER_LEN_BYTES + 1]);
    ASSERT_EQ(3, buffer[PLDM_RESPONSE_HEADER_LEN_BYTES + 2]);
}
