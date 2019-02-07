#include "libpldm/base.h"
#include <gtest/gtest.h>

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
