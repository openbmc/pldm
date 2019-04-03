#include "libpldmresponder/base.hpp"

#include <string.h>

#include <array>

#include "libpldm/base.h"

#include <gtest/gtest.h>

using namespace pldm::responder;

TEST(GetPLDMTypes, testGoodRequest)
{
    std::array<uint8_t, 5> requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    size_t request_payload_length = requestPayload.size();
    auto responseVct = getPLDMTypes(request, request_payload_length);
    // Only base type supported at the moment
    auto response = reinterpret_cast<pldm_msg*>(responseVct.data());
    uint8_t* payload_ptr = response->payload;
    ASSERT_EQ(payload_ptr[0], 0);
    ASSERT_EQ(payload_ptr[1], 1);
    ASSERT_EQ(payload_ptr[2], 0);
}

TEST(GetPLDMCommands, testGoodRequest)
{
    // Only base type supported at the moment, and commands -
    // GetPLDMTypes, GetPLDMCommands
    std::array<uint8_t, 5> requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    size_t request_payload_length = requestPayload.size();
    auto responseVct = getPLDMCommands(request, request_payload_length);
    auto response = reinterpret_cast<pldm_msg*>(responseVct.data());
    uint8_t* payload_ptr = response->payload;
    ASSERT_EQ(payload_ptr[0], 0);
    ASSERT_EQ(payload_ptr[1], 48); // 48 = 0b110000
    ASSERT_EQ(payload_ptr[2], 0);
}

TEST(GetPLDMCommands, testBadRequest)
{
    std::array<uint8_t, 5> requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());

    request->payload[0] = 0xFF;
    size_t request_payload_length = requestPayload.size();
    auto responseVct = getPLDMCommands(request, request_payload_length);
    auto response = reinterpret_cast<pldm_msg*>(responseVct.data());
    uint8_t* payload_ptr = response->payload;
    ASSERT_EQ(payload_ptr[0], PLDM_ERROR_INVALID_PLDM_TYPE);
}
TEST(GetPLDMVersion, testGoodRequest)
{
    std::array<uint8_t, PLDM_GET_VERSION_REQ_BYTES> requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    size_t request_payload_length = requestPayload.size();

    uint8_t pldmType = PLDM_BASE;
    uint32_t transferHandle = 0x0;
    uint8_t flag = PLDM_GET_FIRSTPART;
    uint8_t retFlag = PLDM_START_AND_END;
    ver32_t version = {0xF1, 0xF0, 0xF0, 0x00};

    auto rc =
        encode_get_version_req(0, transferHandle, flag, pldmType, request);

    ASSERT_EQ(0, rc);

    auto responseVct = getPLDMVersion(request, request_payload_length);
    auto response = reinterpret_cast<pldm_msg*>(responseVct.data());

    ASSERT_EQ(response->payload[0], 0);
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]),
                        &transferHandle, sizeof(transferHandle)));
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]) +
                            sizeof(transferHandle),
                        &retFlag, sizeof(flag)));
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]) +
                            sizeof(transferHandle) + sizeof(flag),
                        &version, sizeof(version)));
}
TEST(GetPLDMVersion, testBadRequest)
{
    std::array<uint8_t, PLDM_GET_VERSION_REQ_BYTES> requestPayload{};

    std::array<uint8_t, (PLDM_GET_VERSION_REQ_BYTES - 3)> requestPayloadSmall{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayloadSmall.data());
    size_t request_payload_length = requestPayloadSmall.size();

    uint8_t pldmType = 7;
    uint32_t transferHandle = 0x0;
    uint8_t flag = PLDM_GET_FIRSTPART;

    auto rc =
        encode_get_version_req(0, transferHandle, flag, pldmType, request);

    ASSERT_EQ(0, rc);

    auto responseVct = getPLDMVersion(request, request_payload_length);
    auto response = reinterpret_cast<pldm_msg*>(responseVct.data());

    ASSERT_EQ(response->payload[0], PLDM_ERROR_INVALID_LENGTH);

    request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    request_payload_length = requestPayload.size();

    rc = encode_get_version_req(0, transferHandle, flag, pldmType, request);

    ASSERT_EQ(0, rc);

    responseVct = getPLDMVersion(request, request_payload_length);
    response = reinterpret_cast<pldm_msg*>(responseVct.data());

    ASSERT_EQ(response->payload[0], PLDM_ERROR_INVALID_PLDM_TYPE);
}
