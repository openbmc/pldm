#include "libpldm/base.h"
#include <gtest/gtest.h>
#include <array>
#include <string.h>

TEST(GetPLDMCommands, testEncodeRequest)
{
    std::array<uint8_t,
        REQUEST_HEADER_LEN_BYTES +
        // PLDM version
        sizeof(ver32) +
        // PLDM Type
        sizeof(uint8_t)> buffer{};
    uint8_t pldmType = 0x05;
    ver32 version = 1001; // 1.0.0.1

    auto rc = encode_get_commands_req(0, pldmType, version, buffer.data());
    ASSERT_EQ(rc, OK);
    ASSERT_EQ(0, memcmp(buffer.data() + REQUEST_HEADER_LEN_BYTES, &pldmType,
                    sizeof(pldmType)));
    ASSERT_EQ(
        0,
        memcmp(buffer.data() + REQUEST_HEADER_LEN_BYTES + sizeof(pldmType),
            &version, sizeof(version)));
}

TEST(GetPLDMCommands, testDecodeRequest)
{
    std::array<uint8_t,
        REQUEST_HEADER_LEN_BYTES +
        // PLDM version
        sizeof(ver32) +
        // PLDM Type
        sizeof(uint8_t)> buffer{};
    uint8_t pldmType = 0x05;
    ver32 version = 1001; // 1.0.0.1
    uint8_t pldmTypeOut{};
    ver32 versionOut{};

    memcpy(buffer.data() + REQUEST_HEADER_LEN_BYTES, &pldmType,
        sizeof(pldmType));
    memcpy(buffer.data() + REQUEST_HEADER_LEN_BYTES + sizeof(pldmType),
        &version, sizeof(version));
    auto rc = decode_get_commands_req(buffer.data(), &pldmTypeOut, &versionOut);
    ASSERT_EQ(rc, OK);
    ASSERT_EQ(pldmTypeOut, pldmType);
    ASSERT_EQ(versionOut, version);
}

TEST(GetPLDMCommands, testEncodeResponse)
{
    std::array<uint8_t, RESPONSE_HEADER_LEN_BYTES + MAX_CMDS_PER_TYPE>
        buffer{};
    std::array<uint8_t, MAX_CMDS_PER_TYPE> commands{};
    commands[0] = 1;
    commands[1] = 2;
    commands[2] = 3;

    auto rc = encode_get_commands_resp(0, 0, commands.data(), buffer.data());
    ASSERT_EQ(rc, OK);
    ASSERT_EQ(1, buffer[RESPONSE_HEADER_LEN_BYTES]);
    ASSERT_EQ(2, buffer[RESPONSE_HEADER_LEN_BYTES + 1]);
    ASSERT_EQ(3, buffer[RESPONSE_HEADER_LEN_BYTES + 2]);
}

TEST(GetPLDMTypes, testEncodeResponse)
{
    std::array<uint8_t, RESPONSE_HEADER_LEN_BYTES + MAX_TYPES>
        buffer{};
    std::array<uint8_t, MAX_TYPES> types{};
    types[0] = 1;
    types[1] = 2;
    types[2] = 3;

    auto rc = encode_get_types_resp(0, 0, types.data(), buffer.data());
    ASSERT_EQ(rc, OK);
    ASSERT_EQ(1, buffer[RESPONSE_HEADER_LEN_BYTES]);
    ASSERT_EQ(2, buffer[RESPONSE_HEADER_LEN_BYTES + 1]);
    ASSERT_EQ(3, buffer[RESPONSE_HEADER_LEN_BYTES + 2]);
}

TEST(PackPLDMMessage, BadPathTest)
{
    struct pldm_header_info hdr;
    struct pldm_header_info *hdr_ptr = NULL;
    void *ptr = NULL;
    uint8_t arr[4] = {0x00, 0x00, 0x00, 0x00};

    // PLDM header information pointer is NULL
    auto rc = pack_pldm_message(&arr, hdr_ptr);
    ASSERT_EQ(rc, FAIL);

    // PLDM message pointer is NULL
    rc = pack_pldm_message(ptr, &hdr);
    ASSERT_EQ(rc, FAIL);

    // PLDM header information pointer and PLDM message pointer is NULL
    rc = pack_pldm_message(ptr, hdr_ptr);
    ASSERT_EQ(rc, FAIL);

    // Invalid message type
    hdr.msg_type = (MessageType)3;
    rc = pack_pldm_message(&arr, &hdr);
    ASSERT_EQ(rc, FAIL);

    // Instance ID out of range
    hdr.msg_type = REQUEST;
    hdr.instance = 32;
    rc = pack_pldm_message(&arr, &hdr);
    ASSERT_EQ(rc, FAIL);

    // PLDM type out of range
    hdr.msg_type = REQUEST;
    hdr.instance = 31;
    hdr.pldm_type = 64;
    rc = pack_pldm_message(&arr, &hdr);
    ASSERT_EQ(rc, FAIL);
}

TEST(PackPLDMMessage, RequestMessageGoodPath)
{
    struct pldm_header_info hdr;
    uint8_t arr[3] = {0x00, 0x00, 0x00};

    // Message type is REQUEST and lower range of the field values
    hdr.msg_type = REQUEST;
    hdr.instance = 0;
    hdr.pldm_type = 0;
    hdr.command = 0;

    auto rc = pack_pldm_message(&arr, &hdr);
    ASSERT_EQ(rc, OK);
    ASSERT_EQ(arr[0], 0x80);
    ASSERT_EQ(arr[1], 0X00);
    ASSERT_EQ(arr[2], 0X00);

    // Message type is REQUEST and upper range of the field values
    hdr.instance = 31;
    hdr.pldm_type = 63;
    hdr.command = 255;

    rc = pack_pldm_message(&arr, &hdr);
    ASSERT_EQ(rc, OK);
    ASSERT_EQ(arr[0], 0x9F);
    ASSERT_EQ(arr[1], 0X3F);
    ASSERT_EQ(arr[2], 0XFF);

    // Message type is ASYNC_REQUEST_NOTIFY
    hdr.msg_type = ASYNC_REQUEST_NOTIFY;

    rc = pack_pldm_message(&arr, &hdr);
    ASSERT_EQ(rc, OK);
    ASSERT_EQ(arr[0], 0xDF);
    ASSERT_EQ(arr[1], 0X3F);
    ASSERT_EQ(arr[2], 0XFF);
}

TEST(PackPLDMMessage, ResponseMessageGoodPath)
{
    struct pldm_header_info hdr;
    uint8_t arr[4] = {0x00, 0x00, 0x00, 0x00};

    // Message type is RESPONSE and lower range of the field values
    hdr.msg_type = RESPONSE;
    hdr.instance = 0;
    hdr.pldm_type = 0;
    hdr.command = 0;
    hdr.completion_code = 0;

    auto rc = pack_pldm_message(&arr, &hdr);
    ASSERT_EQ(rc, OK);
    ASSERT_EQ(arr[0], 0x00);
    ASSERT_EQ(arr[1], 0X00);
    ASSERT_EQ(arr[2], 0X00);
    ASSERT_EQ(arr[3], 0X00);

    // Message type is RESPONSE and upper range of the field values
    hdr.instance = 31;
    hdr.pldm_type = 63;
    hdr.command = 255;
    hdr.completion_code = 255;

    rc = pack_pldm_message(&arr, &hdr);
    ASSERT_EQ(rc, OK);
    ASSERT_EQ(arr[0], 0x1F);
    ASSERT_EQ(arr[1], 0X3F);
    ASSERT_EQ(arr[2], 0XFF);
    ASSERT_EQ(arr[3], 0XFF);
}


TEST(UnpackPLDMMessage, BadPathTest)
{
    struct pldm_header_info hdr;
    void *ptr = NULL;
    size_t resp_size = 0;
    size_t offset = 0;
    uint8_t arr[4] = {0x00, 0x00, 0x00, 0x00};

    // PLDM message pointer is NULL
    auto rc = unpack_pldm_message(ptr, 0, &hdr, &offset, &resp_size);
    ASSERT_EQ(rc, -1);

    // Request message and size of PLDM message is less than the PLDM request
    // header size
    arr[0] = 0x80;
    unpack_pldm_message(&arr, 2, &hdr, &offset, &resp_size);
    ASSERT_EQ(rc, -1);

    // Response message and size of PLDM message is less than the PLDM response
    // header size
    arr[0] = 0x00;
    unpack_pldm_message(&arr, 3, &hdr, &offset, &resp_size);
    ASSERT_EQ(rc, -1);
}

TEST(UnpackPLDMMessage, RequestMessageGoodPath)
{
    struct pldm_header_info hdr;
    size_t resp_size = 0;
    size_t offset = 0;

    uint8_t arr[4] = {0x80, 0x00, 0x00, 0x00};

    // Unpack PLDM request message and lower range of field values
    auto rc = unpack_pldm_message(&arr, sizeof(arr), &hdr, &offset, &resp_size);
    ASSERT_EQ(rc, OK);
    ASSERT_EQ(hdr.msg_type, REQUEST);
    ASSERT_EQ(hdr.instance, 0);
    ASSERT_EQ(hdr.pldm_type, 0);
    ASSERT_EQ(hdr.command, 0);
    ASSERT_EQ(offset, PLDM_REQUEST_HEADER_SIZE);
    ASSERT_EQ(resp_size, 1);

    // Unpack PLDM async request message and lower range of field values
    uint8_t arr1[4] = {0xC0, 0x00, 0x00, 0x00};
    rc = unpack_pldm_message(&arr1, sizeof(arr1), &hdr, &offset, &resp_size);
    ASSERT_EQ(rc, OK);
    ASSERT_EQ(hdr.msg_type, ASYNC_REQUEST_NOTIFY);

    // Unpack PLDM request message and upper range of field values
    uint8_t arr2[5] = {0x9F, 0x3F, 0xFF, 0x00, 0x00};
    rc = unpack_pldm_message(&arr2, sizeof(arr2), &hdr, &offset, &resp_size);
    ASSERT_EQ(rc, OK);
    ASSERT_EQ(hdr.msg_type, REQUEST);
    ASSERT_EQ(hdr.instance, 31);
    ASSERT_EQ(hdr.pldm_type, 63);
    ASSERT_EQ(hdr.command, 255);
    ASSERT_EQ(offset, PLDM_REQUEST_HEADER_SIZE);
    ASSERT_EQ(resp_size, 2);
}

TEST(UnpackPLDMMessage, ResponseMessageGoodPath)
{
    struct pldm_header_info hdr;
    size_t resp_size = 0;
    size_t offset = 0;

    uint8_t arr[5] = {0x00, 0x00, 0x00, 0x00, 0x00};

    // Unpack PLDM response message and lower range of field values
    unpack_pldm_message(&arr, sizeof(arr), &hdr, &offset, &resp_size);
    ASSERT_EQ(hdr.msg_type, RESPONSE);
    ASSERT_EQ(hdr.instance, 0);
    ASSERT_EQ(hdr.pldm_type, 0);
    ASSERT_EQ(hdr.command, 0);
    ASSERT_EQ(hdr.completion_code, 0);
    ASSERT_EQ(offset, PLDM_RESPONSE_HEADER_SIZE);
    ASSERT_EQ(resp_size, 1);

    uint8_t arr1[6] = {0x1F, 0x3F, 0xFF, 0xFF, 0x00, 0x00};

    // Unpack PLDM response message and upper range of field values
    unpack_pldm_message(&arr1, sizeof(arr1), &hdr, &offset, &resp_size);
    ASSERT_EQ(hdr.msg_type, RESPONSE);
    ASSERT_EQ(hdr.instance, 31);
    ASSERT_EQ(hdr.pldm_type, 63);
    ASSERT_EQ(hdr.command, 255);
    ASSERT_EQ(hdr.completion_code, 255);
    ASSERT_EQ(offset, PLDM_RESPONSE_HEADER_SIZE);
    ASSERT_EQ(resp_size, 2);
}
