#include <string.h>

#include <array>

#include "libpldm/base.h"

#include <gtest/gtest.h>

TEST(PackPLDMMessage, BadPathTest)
{
    struct pldm_header_info hdr;
    struct pldm_header_info* hdr_ptr = NULL;
    uint8_t* ptr = NULL;
    uint8_t arr[4] = {0x00, 0x00, 0x00, 0x00};

    // PLDM header information pointer is NULL
    auto rc = pack_pldm_header(hdr_ptr, (uint8_t*)&arr);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // PLDM message pointer is NULL
    rc = pack_pldm_header(&hdr, ptr);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // PLDM header information pointer and PLDM message pointer is NULL
    rc = pack_pldm_header(hdr_ptr, ptr);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // RESERVED message type
    hdr.msg_type = RESERVED;
    rc = pack_pldm_header(&hdr, (uint8_t*)&arr);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Instance ID out of range
    hdr.msg_type = REQUEST;
    hdr.instance = 32;
    rc = pack_pldm_header(&hdr, (uint8_t*)&arr);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // PLDM type out of range
    hdr.msg_type = REQUEST;
    hdr.instance = 31;
    hdr.pldm_type = 64;
    rc = pack_pldm_header(&hdr, (uint8_t*)&arr);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_PLDM_TYPE);
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

    auto rc = pack_pldm_header(&hdr, (uint8_t*)&arr);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(arr[0], 0x80);
    ASSERT_EQ(arr[1], 0X00);
    ASSERT_EQ(arr[2], 0X00);

    // Message type is REQUEST and upper range of the field values
    hdr.instance = 31;
    hdr.pldm_type = 63;
    hdr.command = 255;

    rc = pack_pldm_header(&hdr, (uint8_t*)&arr);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(arr[0], 0x9F);
    ASSERT_EQ(arr[1], 0X3F);
    ASSERT_EQ(arr[2], 0XFF);

    // Message type is ASYNC_REQUEST_NOTIFY
    hdr.msg_type = ASYNC_REQUEST_NOTIFY;

    rc = pack_pldm_header(&hdr, (uint8_t*)&arr);
    ASSERT_EQ(rc, PLDM_SUCCESS);
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

    auto rc = pack_pldm_header(&hdr, (uint8_t*)&arr);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(arr[0], 0x00);
    ASSERT_EQ(arr[1], 0X00);
    ASSERT_EQ(arr[2], 0X00);
    ASSERT_EQ(arr[3], 0X00);

    // Message type is RESPONSE and upper range of the field values
    hdr.instance = 31;
    hdr.pldm_type = 63;
    hdr.command = 255;
    hdr.completion_code = 255;

    rc = pack_pldm_header(&hdr, (uint8_t*)&arr);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(arr[0], 0x1F);
    ASSERT_EQ(arr[1], 0X3F);
    ASSERT_EQ(arr[2], 0XFF);
    ASSERT_EQ(arr[3], 0XFF);
}

TEST(UnpackPLDMMessage, BadPathTest)
{
    struct pldm_header_info hdr;
    uint8_t* ptr = NULL;
    size_t resp_size = 0;
    size_t offset = 0;
    uint8_t arr[4] = {0x00, 0x00, 0x00, 0x00};

    // PLDM message pointer is NULL
    auto rc = unpack_pldm_header(ptr, 0, &hdr, &offset, &resp_size);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Request message and size of PLDM message is less than the PLDM request
    // header size
    arr[0] = 0x80;
    rc = unpack_pldm_header((uint8_t*)&arr, 2, &hdr, &offset, &resp_size);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    // Response message and size of PLDM message is less than the PLDM response
    // header size
    arr[0] = 0x00;
    rc = unpack_pldm_header((uint8_t*)&arr, 3, &hdr, &offset, &resp_size);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(UnpackPLDMMessage, RequestMessageGoodPath)
{
    struct pldm_header_info hdr;
    size_t resp_size = 0;
    size_t offset = 0;

    uint8_t arr[4] = {0x80, 0x00, 0x00, 0x00};

    // Unpack PLDM request message and lower range of field values
    auto rc = unpack_pldm_header((uint8_t*)&arr, sizeof(arr), &hdr, &offset,
                                 &resp_size);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(hdr.msg_type, REQUEST);
    ASSERT_EQ(hdr.instance, 0);
    ASSERT_EQ(hdr.pldm_type, 0);
    ASSERT_EQ(hdr.command, 0);
    ASSERT_EQ(offset, PLDM_REQUEST_HEADER_LEN_BYTES);
    ASSERT_EQ(resp_size, 1);

    // Unpack PLDM async request message and lower range of field values
    uint8_t arr1[4] = {0xC0, 0x00, 0x00, 0x00};
    rc = unpack_pldm_header((uint8_t*)&arr1, sizeof(arr1), &hdr, &offset,
                            &resp_size);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(hdr.msg_type, ASYNC_REQUEST_NOTIFY);

    // Unpack PLDM request message and upper range of field values
    uint8_t arr2[5] = {0x9F, 0x3F, 0xFF, 0x00, 0x00};
    rc = unpack_pldm_header((uint8_t*)&arr2, sizeof(arr2), &hdr, &offset,
                            &resp_size);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(hdr.msg_type, REQUEST);
    ASSERT_EQ(hdr.instance, 31);
    ASSERT_EQ(hdr.pldm_type, 63);
    ASSERT_EQ(hdr.command, 255);
    ASSERT_EQ(offset, PLDM_REQUEST_HEADER_LEN_BYTES);
    ASSERT_EQ(resp_size, 2);
}

TEST(UnpackPLDMMessage, ResponseMessageGoodPath)
{
    struct pldm_header_info hdr;
    size_t resp_size = 0;
    size_t offset = 0;

    uint8_t arr[5] = {0x00, 0x00, 0x00, 0x00, 0x00};

    // Unpack PLDM response message and lower range of field values
    auto rc = unpack_pldm_header((uint8_t*)&arr, sizeof(arr), &hdr, &offset,
                                 &resp_size);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(hdr.msg_type, RESPONSE);
    ASSERT_EQ(hdr.instance, 0);
    ASSERT_EQ(hdr.pldm_type, 0);
    ASSERT_EQ(hdr.command, 0);
    ASSERT_EQ(hdr.completion_code, 0);
    ASSERT_EQ(offset, PLDM_RESPONSE_HEADER_LEN_BYTES);
    ASSERT_EQ(resp_size, 1);

    uint8_t arr1[6] = {0x1F, 0x3F, 0xFF, 0xFF, 0x00, 0x00};

    // Unpack PLDM response message and upper range of field values
    rc = unpack_pldm_header((uint8_t*)&arr1, sizeof(arr1), &hdr, &offset,
                            &resp_size);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(hdr.msg_type, RESPONSE);
    ASSERT_EQ(hdr.instance, 31);
    ASSERT_EQ(hdr.pldm_type, 63);
    ASSERT_EQ(hdr.command, 255);
    ASSERT_EQ(hdr.completion_code, 255);
    ASSERT_EQ(offset, PLDM_RESPONSE_HEADER_LEN_BYTES);
    ASSERT_EQ(resp_size, 2);
}

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
