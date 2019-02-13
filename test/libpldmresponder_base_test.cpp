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

TEST(GetPLDMVersion, testGoodRequest)
{
    pldm_msg_t request{};
    pldm_msg_t response{};
    std::array<uint8_t, PLDM_GET_VERSION_RESP_DATA_BYTES> responsePayload{};
    response.payload = responsePayload.data();

    std::array<uint8_t, 6> requestPayload{};
    request.payload = requestPayload.data();

    uint8_t pldmType = PLDM_BASE;
    uint32_t transferHandle = 0x0;
    uint8_t flag = PLDM_GET_FIRSTPART;
    uint8_t retFlag = PLDM_START_AND_END;
    struct pldm_version_t version = {0xF1, 0xF0, 0xF0, 0x00};

    auto rc =
        encode_get_version_req(0, transferHandle, flag, pldmType, &request);

    ASSERT_EQ(rc, 0);

    getPLDMVersion(&request, requestPayload.size(), &response);

    ASSERT_EQ(0, memcmp(response.payload + sizeof(response.payload[0]),
                        &transferHandle, sizeof(transferHandle)));
    ASSERT_EQ(0, memcmp(response.payload + sizeof(response.payload[0]) +
                            sizeof(transferHandle),
                        &retFlag, sizeof(flag)));
    ASSERT_EQ(0, memcmp(response.payload + sizeof(response.payload[0]) +
                            sizeof(transferHandle) + sizeof(flag),
                        &version, sizeof(version)));
}
