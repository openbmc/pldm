#include "libpldmresponder/base.hpp"
#include "registration.hpp"

#include <string.h>

#include <array>

#include "libpldm/base.h"

#include <gtest/gtest.h>

using namespace pldm;
using namespace pldm::responder;

TEST(GetPLDMTypes, testGoodRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr)> requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    // payload length will be 0 in this case
    size_t requestPayloadLength = 0;
    Request req{0, request};
    Interfaces intfs{};
    auto response = getPLDMTypes(intfs, req, requestPayloadLength);
    // Only base type supported at the moment
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    uint8_t* payload_ptr = responsePtr->payload;
    ASSERT_EQ(payload_ptr[0], 0);
    ASSERT_EQ(payload_ptr[1], 13);
    ASSERT_EQ(payload_ptr[2], 0);
}

TEST(GetPLDMCommands, testGoodRequest)
{
    // Only base type supported at the moment, and commands -
    // GetPLDMTypes, GetPLDMCommands
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_COMMANDS_REQ_BYTES>
        requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);
    Request req{0, request};
    Interfaces intfs{};
    auto response = getPLDMCommands(intfs, req, requestPayloadLength);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    uint8_t* payload_ptr = responsePtr->payload;
    ASSERT_EQ(payload_ptr[0], 0);
    ASSERT_EQ(payload_ptr[1], 56); // 56 = 0b111000
    ASSERT_EQ(payload_ptr[2], 0);
}

TEST(GetPLDMCommands, testBadRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_COMMANDS_REQ_BYTES>
        requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());

    request->payload[0] = 0xFF;
    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);
    Request req{0, request};
    Interfaces intfs{};
    auto response = getPLDMCommands(intfs, req, requestPayloadLength);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    uint8_t* payload_ptr = responsePtr->payload;
    ASSERT_EQ(payload_ptr[0], PLDM_ERROR_INVALID_PLDM_TYPE);
}
TEST(GetPLDMVersion, testGoodRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_VERSION_REQ_BYTES>
        requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    uint8_t pldmType = PLDM_BASE;
    uint32_t transferHandle = 0x0;
    uint8_t flag = PLDM_GET_FIRSTPART;
    uint8_t retFlag = PLDM_START_AND_END;
    ver32_t version = {0xF1, 0xF0, 0xF0, 0x00};

    auto rc =
        encode_get_version_req(0, transferHandle, flag, pldmType, request);

    ASSERT_EQ(0, rc);

    Request req{0, request};
    Interfaces intfs{};
    auto response = getPLDMVersion(intfs, req, requestPayloadLength);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    ASSERT_EQ(responsePtr->payload[0], 0);
    ASSERT_EQ(0, memcmp(responsePtr->payload + sizeof(responsePtr->payload[0]),
                        &transferHandle, sizeof(transferHandle)));
    ASSERT_EQ(0, memcmp(responsePtr->payload + sizeof(responsePtr->payload[0]) +
                            sizeof(transferHandle),
                        &retFlag, sizeof(flag)));
    ASSERT_EQ(0, memcmp(responsePtr->payload + sizeof(responsePtr->payload[0]) +
                            sizeof(transferHandle) + sizeof(flag),
                        &version, sizeof(version)));
}
TEST(GetPLDMVersion, testBadRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_VERSION_REQ_BYTES>
        requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    uint8_t pldmType = 7;
    uint32_t transferHandle = 0x0;
    uint8_t flag = PLDM_GET_FIRSTPART;

    auto rc =
        encode_get_version_req(0, transferHandle, flag, pldmType, request);

    ASSERT_EQ(0, rc);

    Interfaces intfs{};
    Request req1{0, request};
    auto response = getPLDMVersion(intfs, req1, requestPayloadLength - 1);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    ASSERT_EQ(responsePtr->payload[0], PLDM_ERROR_INVALID_LENGTH);

    request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    rc = encode_get_version_req(0, transferHandle, flag, pldmType, request);

    ASSERT_EQ(0, rc);

    Request req2{0, request};
    response = getPLDMVersion(intfs, req2, requestPayloadLength);
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    ASSERT_EQ(responsePtr->payload[0], PLDM_ERROR_INVALID_PLDM_TYPE);
}
