#include <string.h>

#include <array>

#include "libpldm/base.h"

#include <gtest/gtest.h>

TEST(PackPLDMMessage, BadPathTest)
{
    struct pldm_header_info hdr;
    struct pldm_header_info* hdr_ptr = NULL;
    struct pldm_msg_t msg{};

    // PLDM header information pointer is NULL
    auto rc = pack_pldm_header(hdr_ptr, &msg);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // PLDM message pointer is NULL
    rc = pack_pldm_header(&hdr, nullptr);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // PLDM header information pointer and PLDM message pointer is NULL
    rc = pack_pldm_header(hdr_ptr, nullptr);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // RESERVED message type
    hdr.msg_type = RESERVED;
    rc = pack_pldm_header(&hdr, &msg);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Instance ID out of range
    hdr.msg_type = REQUEST;
    hdr.instance = 32;
    rc = pack_pldm_header(&hdr, &msg);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // PLDM type out of range
    hdr.msg_type = REQUEST;
    hdr.instance = 31;
    hdr.pldm_type = 64;
    rc = pack_pldm_header(&hdr, &msg);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_PLDM_TYPE);
}

TEST(PackPLDMMessage, RequestMessageGoodPath)
{
    struct pldm_header_info hdr;
    struct pldm_msg_t msg{};

    // Message type is REQUEST and lower range of the field values
    hdr.msg_type = REQUEST;
    hdr.instance = 0;
    hdr.pldm_type = 0;
    hdr.command = 0;

    auto rc = pack_pldm_header(&hdr, &msg);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(msg.request, 1);
    ASSERT_EQ(msg.datagram, 0);
    ASSERT_EQ(msg.instance_id, 0);
    ASSERT_EQ(msg.type, 0);
    ASSERT_EQ(msg.command, 0);

    // Message type is REQUEST and upper range of the field values
    hdr.instance = 31;
    hdr.pldm_type = 63;
    hdr.command = 255;

    rc = pack_pldm_header(&hdr, &msg);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(msg.request, 1);
    ASSERT_EQ(msg.datagram, 0);
    ASSERT_EQ(msg.instance_id, 31);
    ASSERT_EQ(msg.type, 63);
    ASSERT_EQ(msg.command, 255);

    // Message type is ASYNC_REQUEST_NOTIFY
    hdr.msg_type = ASYNC_REQUEST_NOTIFY;

    rc = pack_pldm_header(&hdr, &msg);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(msg.request, 1);
    ASSERT_EQ(msg.datagram, 1);
    ASSERT_EQ(msg.instance_id, 31);
    ASSERT_EQ(msg.type, 63);
    ASSERT_EQ(msg.command, 255);
}

TEST(PackPLDMMessage, ResponseMessageGoodPath)
{
    struct pldm_header_info hdr;
    struct pldm_msg_t msg{};

    // Message type is RESPONSE and lower range of the field values
    hdr.msg_type = RESPONSE;
    hdr.instance = 0;
    hdr.pldm_type = 0;
    hdr.command = 0;

    auto rc = pack_pldm_header(&hdr, &msg);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(msg.request, 0);
    ASSERT_EQ(msg.datagram, 0);
    ASSERT_EQ(msg.instance_id, 0);
    ASSERT_EQ(msg.type, 0);
    ASSERT_EQ(msg.command, 0);

    // Message type is RESPONSE and upper range of the field values
    hdr.instance = 31;
    hdr.pldm_type = 63;
    hdr.command = 255;

    rc = pack_pldm_header(&hdr, &msg);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(msg.request, 0);
    ASSERT_EQ(msg.datagram, 0);
    ASSERT_EQ(msg.instance_id, 31);
    ASSERT_EQ(msg.type, 63);
    ASSERT_EQ(msg.command, 255);
}

TEST(UnpackPLDMMessage, BadPathTest)
{
    struct pldm_header_info hdr;
    size_t resp_size = 0;

    // PLDM message pointer is NULL
    auto rc = unpack_pldm_header(nullptr, 0, &hdr, &resp_size);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(UnpackPLDMMessage, RequestMessageGoodPath)
{
    struct pldm_header_info hdr;
    struct pldm_msg_t msg{};
    size_t resp_size = 0;

    // Unpack PLDM request message and lower range of field values
    msg.request = 1;
    auto rc = unpack_pldm_header(&msg, sizeof(msg) - sizeof(uint8_t*), &hdr,
                                 &resp_size);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(hdr.msg_type, REQUEST);
    ASSERT_EQ(hdr.instance, 0);
    ASSERT_EQ(hdr.pldm_type, 0);
    ASSERT_EQ(hdr.command, 0);
    ASSERT_EQ(resp_size, 0);

    // Unpack PLDM async request message and lower range of field values
    msg.datagram = 1;
    rc = unpack_pldm_header(&msg, sizeof(msg) - sizeof(uint8_t*), &hdr,
                            &resp_size);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(hdr.msg_type, ASYNC_REQUEST_NOTIFY);

    // Unpack PLDM request message and upper range of field values
    msg.datagram = 0;
    msg.instance_id = 31;
    msg.type = 63;
    msg.command = 255;
    rc = unpack_pldm_header(&msg, sizeof(msg) - sizeof(uint8_t*) + 2, &hdr,
                            &resp_size);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(hdr.msg_type, REQUEST);
    ASSERT_EQ(hdr.instance, 31);
    ASSERT_EQ(hdr.pldm_type, 63);
    ASSERT_EQ(hdr.command, 255);
    ASSERT_EQ(resp_size, 2);
}

TEST(UnpackPLDMMessage, ResponseMessageGoodPath)
{
    struct pldm_header_info hdr;
    struct pldm_msg_t msg{};
    size_t resp_size = 0;

    // Unpack PLDM response message and lower range of field values
    auto rc = unpack_pldm_header(&msg, sizeof(msg) - sizeof(uint8_t*), &hdr,
                                 &resp_size);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(hdr.msg_type, RESPONSE);
    ASSERT_EQ(hdr.instance, 0);
    ASSERT_EQ(hdr.pldm_type, 0);
    ASSERT_EQ(hdr.command, 0);
    ASSERT_EQ(resp_size, 0);

    // Unpack PLDM response message and upper range of field values
    msg.instance_id = 31;
    msg.type = 63;
    msg.command = 255;
    rc = unpack_pldm_header(&msg, sizeof(msg) - sizeof(uint8_t*) + 2, &hdr,
                            &resp_size);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(hdr.msg_type, RESPONSE);
    ASSERT_EQ(hdr.instance, 31);
    ASSERT_EQ(hdr.pldm_type, 63);
    ASSERT_EQ(hdr.command, 255);
    ASSERT_EQ(resp_size, 2);
}

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
