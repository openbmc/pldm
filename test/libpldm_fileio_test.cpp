#include <string.h>

#include <array>

#include "libpldm/base.h"
#include "libpldm/file_io.h"

#include <gtest/gtest.h>

TEST(ReadFileIntoMemory, testGoodDecodeRequest)
{
    std::array<uint8_t, PLDM_READ_FILE_MEM_REQ_BYTES> requestMsg{};

    pldm_msg_payload request{};
    request.payload = requestMsg.data();
    request.payload_length = requestMsg.size();

    // Random value for fileHandle, offset, length, address
    uint32_t fileHandle = 0x12345678;
    uint32_t offset = 0x87654321;
    uint32_t length = 0x13245768;
    uint64_t address = 0x124356879ACBDE0F;

    memcpy(request.payload, &fileHandle, sizeof(fileHandle));
    memcpy(request.payload + sizeof(fileHandle), &offset, sizeof(offset));
    memcpy(request.payload + sizeof(fileHandle) + sizeof(offset), &length,
           sizeof(length));
    memcpy(request.payload + sizeof(fileHandle) + sizeof(offset) +
               sizeof(length),
           &address, sizeof(address));

    uint32_t retFileHandle = 0;
    uint32_t retOffset = 0;
    uint32_t retLength = 0;
    uint64_t retAddress = 0;

    // Invoke decode the read file memory request
    auto rc = decode_read_file_memory_req(&request, &retFileHandle, &retOffset,
                                          &retLength, &retAddress);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(fileHandle, retFileHandle);
    ASSERT_EQ(offset, retOffset);
    ASSERT_EQ(length, retLength);
    ASSERT_EQ(address, retAddress);
}

TEST(ReadFileIntoMemory, testBadDecodeRequest)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint64_t address = 0;

    // Request payload message is missing
    auto rc = decode_read_file_memory_req(NULL, &fileHandle, &offset, &length,
                                          &address);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    std::array<uint8_t, PLDM_READ_FILE_MEM_REQ_BYTES> requestMsg{};

    pldm_msg_payload request{};
    request.payload = requestMsg.data();
    request.payload_length = requestMsg.size();

    // Address is NULL
    rc = decode_read_file_memory_req(&request, &fileHandle, &offset, &length,
                                     NULL);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    request.payload_length = 0;

    // Payload length is invalid
    rc = decode_read_file_memory_req(&request, &fileHandle, &offset, &length,
                                     &address);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(ReadFileIntoMemory, testEncodeResponse)
{
    pldm_msg response{};
    std::array<uint8_t, PLDM_READ_FILE_MEM_RESP_BYTES> responseMsg{};
    uint32_t length = 0xFF00EE11;

    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();

    auto rc = encode_read_file_memory_resp(0, PLDM_SUCCESS, length, &response);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(response.body.payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0,
              memcmp(response.body.payload + sizeof(response.body.payload[0]),
                     &length, sizeof(length)));
}
