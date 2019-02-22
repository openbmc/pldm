#include <string.h>

#include <array>

#include "libpldm/base.h"

#include <gtest/gtest.h>

TEST(PackPLDMMessage, BadPathTest)
{
    struct pldm_header_info hdr;
    struct pldm_header_info* hdr_ptr = NULL;
    pldm_msg_hdr msg{};

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
    hdr.msg_type = PLDM_RESERVED;
    rc = pack_pldm_header(&hdr, &msg);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Instance ID out of range
    hdr.msg_type = PLDM_REQUEST;
    hdr.instance = 32;
    rc = pack_pldm_header(&hdr, &msg);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // PLDM type out of range
    hdr.msg_type = PLDM_REQUEST;
    hdr.instance = 31;
    hdr.pldm_type = 64;
    rc = pack_pldm_header(&hdr, &msg);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_PLDM_TYPE);
}

TEST(PackPLDMMessage, RequestMessageGoodPath)
{
    struct pldm_header_info hdr;
    pldm_msg_hdr msg{};

    // Message type is REQUEST and lower range of the field values
    hdr.msg_type = PLDM_REQUEST;
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

    // Message type is PLDM_ASYNC_REQUEST_NOTIFY
    hdr.msg_type = PLDM_ASYNC_REQUEST_NOTIFY;

    rc = pack_pldm_header(&hdr, &msg);
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
    pldm_msg_hdr msg{};

    // Message type is PLDM_RESPONSE and lower range of the field values
    hdr.msg_type = PLDM_RESPONSE;
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

    // Message type is PLDM_RESPONSE and upper range of the field values
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

    // PLDM message pointer is NULL
    auto rc = unpack_pldm_header(nullptr, &hdr);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(UnpackPLDMMessage, RequestMessageGoodPath)
{
    struct pldm_header_info hdr;
    pldm_msg_hdr msg{};

    // Unpack PLDM request message and lower range of field values
    msg.request = 1;
    auto rc = unpack_pldm_header(&msg, &hdr);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(hdr.msg_type, PLDM_REQUEST);
    ASSERT_EQ(hdr.instance, 0);
    ASSERT_EQ(hdr.pldm_type, 0);
    ASSERT_EQ(hdr.command, 0);

    // Unpack PLDM async request message and lower range of field values
    msg.datagram = 1;
    rc = unpack_pldm_header(&msg, &hdr);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(hdr.msg_type, PLDM_ASYNC_REQUEST_NOTIFY);

    // Unpack PLDM request message and upper range of field values
    msg.datagram = 0;
    msg.instance_id = 31;
    msg.type = 63;
    msg.command = 255;
    rc = unpack_pldm_header(&msg, &hdr);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(hdr.msg_type, PLDM_REQUEST);
    ASSERT_EQ(hdr.instance, 31);
    ASSERT_EQ(hdr.pldm_type, 63);
    ASSERT_EQ(hdr.command, 255);
}

TEST(UnpackPLDMMessage, ResponseMessageGoodPath)
{
    struct pldm_header_info hdr;
    pldm_msg_hdr msg{};

    // Unpack PLDM response message and lower range of field values
    auto rc = unpack_pldm_header(&msg, &hdr);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(hdr.msg_type, PLDM_RESPONSE);
    ASSERT_EQ(hdr.instance, 0);
    ASSERT_EQ(hdr.pldm_type, 0);
    ASSERT_EQ(hdr.command, 0);

    // Unpack PLDM response message and upper range of field values
    msg.instance_id = 31;
    msg.type = 63;
    msg.command = 255;
    rc = unpack_pldm_header(&msg, &hdr);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(hdr.msg_type, PLDM_RESPONSE);
    ASSERT_EQ(hdr.instance, 31);
    ASSERT_EQ(hdr.pldm_type, 63);
    ASSERT_EQ(hdr.command, 255);
}

TEST(GetPLDMCommands, testEncodeRequest)
{
    uint8_t pldmType = 0x05;
    ver32_t version{0xFF, 0xFF, 0xFF, 0xFF};
    std::array<uint8_t, PLDM_GET_COMMANDS_REQ_BYTES> requestMsg{};
    pldm_msg request{};
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
    ver32_t version{0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t pldmTypeOut{};
    ver32_t versionOut{0xFF, 0xFF, 0xFF, 0xFF};
    std::array<uint8_t, PLDM_GET_COMMANDS_REQ_BYTES> requestMsg{};
    pldm_msg_payload request{};
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
    pldm_msg response{};
    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();
    std::array<bitfield8_t, PLDM_MAX_CMDS_PER_TYPE / 8> commands{};
    commands[0].byte = 1;
    commands[1].byte = 2;
    commands[2].byte = 3;

    auto rc =
        encode_get_commands_resp(0, PLDM_SUCCESS, commands.data(), &response);
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
    pldm_msg response{};
    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();
    std::array<bitfield8_t, PLDM_MAX_TYPES / 8> types{};
    types[0].byte = 1;
    types[1].byte = 2;
    types[2].byte = 3;

    auto rc = encode_get_types_resp(0, PLDM_SUCCESS, types.data(), &response);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completionCode, response.body.payload[0]);
    ASSERT_EQ(1, response.body.payload[1]);
    ASSERT_EQ(2, response.body.payload[2]);
    ASSERT_EQ(3, response.body.payload[3]);
}

TEST(GetPLDMTypes, testDecodeResponse)
{
    std::array<uint8_t, PLDM_GET_TYPES_RESP_BYTES> responseMsg{};
    pldm_msg_payload response{};
    response.payload = responseMsg.data();
    response.payload_length = responseMsg.size();
    response.payload[1] = 1;
    response.payload[2] = 2;
    response.payload[3] = 3;
    std::array<bitfield8_t, PLDM_MAX_TYPES / 8> outTypes{};
    uint8_t completion_code;

    auto rc =
        decode_get_types_resp(&response, &completion_code, outTypes.data());

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(response.payload[1], outTypes[0].byte);
    ASSERT_EQ(response.payload[2], outTypes[1].byte);
    ASSERT_EQ(response.payload[3], outTypes[2].byte);
}

TEST(GetPLDMCommands, testDecodeResponse)
{
    std::array<uint8_t, PLDM_MAX_CMDS_PER_TYPE> responseMsg{};
    pldm_msg_payload response{};
    response.payload = responseMsg.data();
    response.payload_length = responseMsg.size();
    response.payload[1] = 1;
    response.payload[2] = 2;
    response.payload[3] = 3;
    std::array<bitfield8_t, PLDM_MAX_CMDS_PER_TYPE / 8> outTypes{};
    uint8_t completion_code;

    auto rc =
        decode_get_commands_resp(&response, &completion_code, outTypes.data());

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(response.payload[1], outTypes[0].byte);
    ASSERT_EQ(response.payload[2], outTypes[1].byte);
    ASSERT_EQ(response.payload[3], outTypes[2].byte);
}

TEST(GetPLDMVersion, testGoodEncodeRequest)
{
    std::array<uint8_t, PLDM_GET_VERSION_REQ_BYTES> requestMsg{};
    pldm_msg request{};
    request.body.payload = requestMsg.data();
    request.body.payload_length = requestMsg.size();
    uint8_t pldmType = 0x03;
    uint32_t transferHandle = 0x0;
    uint8_t opFlag = 0x01;

    auto rc =
        encode_get_version_req(0, transferHandle, opFlag, pldmType, &request);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(request.body.payload, &transferHandle,
                        sizeof(transferHandle)));
    ASSERT_EQ(0, memcmp(request.body.payload + sizeof(transferHandle), &opFlag,
                        sizeof(opFlag)));
    ASSERT_EQ(0, memcmp(request.body.payload + sizeof(transferHandle) +
                            sizeof(opFlag),
                        &pldmType, sizeof(pldmType)));
}

TEST(GetPLDMVersion, testBadEncodeRequest)
{
    uint8_t pldmType = 0x03;
    uint32_t transferHandle = 0x0;
    uint8_t opFlag = 0x01;

    auto rc =
        encode_get_version_req(0, transferHandle, opFlag, pldmType, nullptr);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(GetPLDMVersion, testEncodeResponse)
{
    pldm_msg response{};
    uint8_t completionCode = 0;
    uint32_t transferHandle = 0;
    uint8_t flag = PLDM_START_AND_END;
    std::array<uint8_t, PLDM_GET_VERSION_RESP_BYTES> responseMsg{};
    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();
    ver32_t version = {0xFF, 0xFF, 0xFF, 0xFF};

    auto rc = encode_get_version_resp(0, PLDM_SUCCESS, 0, PLDM_START_AND_END,
                                      &version, sizeof(ver32_t), &response);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completionCode, response.body.payload[0]);
    ASSERT_EQ(0,
              memcmp(response.body.payload + sizeof(response.body.payload[0]),
                     &transferHandle, sizeof(transferHandle)));
    ASSERT_EQ(0,
              memcmp(response.body.payload + sizeof(response.body.payload[0]) +
                         sizeof(transferHandle),
                     &flag, sizeof(flag)));
    ASSERT_EQ(0,
              memcmp(response.body.payload + sizeof(response.body.payload[0]) +
                         sizeof(transferHandle) + sizeof(flag),
                     &version, sizeof(version)));
}

TEST(GetPLDMVersion, testDecodeRequest)
{
    std::array<uint8_t, PLDM_GET_VERSION_REQ_BYTES> requestMsg{};
    pldm_msg_payload request{};
    request.payload = requestMsg.data();
    request.payload_length = requestMsg.size();
    uint32_t transferHandle = 0x0;
    uint32_t retTransferHandle = 0x0;
    uint8_t flag = PLDM_GET_FIRSTPART;
    uint8_t retFlag = PLDM_GET_FIRSTPART;
    uint8_t pldmType = PLDM_BASE;
    uint8_t retType = PLDM_BASE;

    memcpy(request.payload, &transferHandle, sizeof(transferHandle));
    memcpy(request.payload + sizeof(transferHandle), &flag, sizeof(flag));
    memcpy(request.payload + sizeof(transferHandle) + sizeof(flag), &pldmType,
           sizeof(pldmType));

    auto rc = decode_get_version_req(&request, &retTransferHandle, &retFlag,
                                     &retType);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(transferHandle, retTransferHandle);
    ASSERT_EQ(flag, retFlag);
    ASSERT_EQ(pldmType, retType);
}

TEST(GetPLDMVersion, testDecodeResponse)
{
    std::array<uint8_t, PLDM_GET_VERSION_RESP_BYTES> responseMsg{};
    pldm_msg_payload response{};
    response.payload = responseMsg.data();
    response.payload_length = responseMsg.size();
    uint32_t transferHandle = 0x0;
    uint32_t retTransferHandle = 0x0;
    uint8_t flag = PLDM_START_AND_END;
    uint8_t retFlag = PLDM_START_AND_END;
    uint8_t completionCode = 0;
    ver32_t version = {0xFF, 0xFF, 0xFF, 0xFF};
    ver32_t versionOut;
    uint8_t completion_code;

    memcpy(response.payload + sizeof(completionCode), &transferHandle,
           sizeof(transferHandle));
    memcpy(response.payload + sizeof(completionCode) + sizeof(transferHandle),
           &flag, sizeof(flag));
    memcpy(response.payload + sizeof(completionCode) + sizeof(transferHandle) +
               sizeof(flag),
           &version, sizeof(version));

    auto rc = decode_get_version_resp(
        &response, &completion_code, &retTransferHandle, &retFlag, &versionOut);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(transferHandle, retTransferHandle);
    ASSERT_EQ(flag, retFlag);

    ASSERT_EQ(versionOut.major, version.major);
    ASSERT_EQ(versionOut.minor, version.minor);
    ASSERT_EQ(versionOut.update, version.update);
    ASSERT_EQ(versionOut.alpha, version.alpha);
}
