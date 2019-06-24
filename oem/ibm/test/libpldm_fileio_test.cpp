#include <string.h>

#include <array>

#include "libpldm/base.h"
#include "libpldm/file_io.h"

#include <gtest/gtest.h>

TEST(ReadWriteFileMemory, testGoodDecodeRequest)
{
    std::array<uint8_t, PLDM_RW_FILE_MEM_REQ_BYTES> requestMsg{};

    // Random value for fileHandle, offset, length, address
    uint32_t fileHandle = 0x12345678;
    uint32_t offset = 0x87654321;
    uint32_t length = 0x13245768;
    uint64_t address = 0x124356879ACBDE0F;

    memcpy(requestMsg.data(), &fileHandle, sizeof(fileHandle));
    memcpy(requestMsg.data() + sizeof(fileHandle), &offset, sizeof(offset));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset), &length,
           sizeof(length));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset) +
               sizeof(length),
           &address, sizeof(address));

    uint32_t retFileHandle = 0;
    uint32_t retOffset = 0;
    uint32_t retLength = 0;
    uint64_t retAddress = 0;

    // Invoke decode the read file memory request
    auto rc = decode_rw_file_memory_req(requestMsg.data(), requestMsg.size(),
                                        &retFileHandle, &retOffset, &retLength,
                                        &retAddress);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(fileHandle, retFileHandle);
    ASSERT_EQ(offset, retOffset);
    ASSERT_EQ(length, retLength);
    ASSERT_EQ(address, retAddress);
}

TEST(ReadWriteFileMemory, testBadDecodeRequest)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint64_t address = 0;

    // Request payload message is missing
    auto rc = decode_rw_file_memory_req(NULL, 0, &fileHandle, &offset, &length,
                                        &address);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    std::array<uint8_t, PLDM_RW_FILE_MEM_REQ_BYTES> requestMsg{};

    // Address is NULL
    rc = decode_rw_file_memory_req(requestMsg.data(), requestMsg.size(),
                                   &fileHandle, &offset, &length, NULL);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Payload length is invalid
    rc = decode_rw_file_memory_req(requestMsg.data(), 0, &fileHandle, &offset,
                                   &length, &address);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(ReadWriteFileMemory, testGoodEncodeResponse)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_RW_FILE_MEM_RESP_BYTES>
        responseMsg{};
    uint32_t length = 0xFF00EE11;
    pldm_msg* response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    // ReadFileIntoMemory
    auto rc = encode_rw_file_memory_resp(0, PLDM_READ_FILE_INTO_MEMORY,
                                         PLDM_SUCCESS, length, response);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(response->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(response->hdr.instance_id, 0);
    ASSERT_EQ(response->hdr.type, PLDM_IBM_OEM_TYPE);
    ASSERT_EQ(response->hdr.command, PLDM_READ_FILE_INTO_MEMORY);
    ASSERT_EQ(response->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]),
                        &length, sizeof(length)));

    // WriteFileFromMemory
    rc = encode_rw_file_memory_resp(0, PLDM_WRITE_FILE_FROM_MEMORY,
                                    PLDM_SUCCESS, length, response);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(response->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(response->hdr.instance_id, 0);
    ASSERT_EQ(response->hdr.type, PLDM_IBM_OEM_TYPE);
    ASSERT_EQ(response->hdr.command, PLDM_WRITE_FILE_FROM_MEMORY);
    ASSERT_EQ(response->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]),
                        &length, sizeof(length)));
}

TEST(ReadWriteFileMemory, testBadEncodeResponse)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_RW_FILE_MEM_RESP_BYTES>
        responseMsg{};
    uint32_t length = 0;
    pldm_msg* response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    // ReadFileIntoMemory
    auto rc = encode_rw_file_memory_resp(0, PLDM_READ_FILE_INTO_MEMORY,
                                         PLDM_ERROR, length, response);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(response->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(response->hdr.instance_id, 0);
    ASSERT_EQ(response->hdr.type, PLDM_IBM_OEM_TYPE);
    ASSERT_EQ(response->hdr.command, PLDM_READ_FILE_INTO_MEMORY);
    ASSERT_EQ(response->payload[0], PLDM_ERROR);

    // WriteFileFromMemory
    rc = encode_rw_file_memory_resp(0, PLDM_WRITE_FILE_FROM_MEMORY, PLDM_ERROR,
                                    length, response);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(response->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(response->hdr.instance_id, 0);
    ASSERT_EQ(response->hdr.type, PLDM_IBM_OEM_TYPE);
    ASSERT_EQ(response->hdr.command, PLDM_WRITE_FILE_FROM_MEMORY);
    ASSERT_EQ(response->payload[0], PLDM_ERROR);
}

TEST(ReadWriteFile, testGoodDecodeRequest)
{
    // Good decode request test for read file
    std::array<uint8_t, PLDM_READ_FILE_REQ_BYTES> requestMsg{};

    // Random value for fileHandle, offset and length
    uint32_t fileHandle = 0x12345678;
    uint32_t offset = 0x87654321;
    uint32_t length = 0x13245768;

    memcpy(requestMsg.data(), &fileHandle, sizeof(fileHandle));
    memcpy(requestMsg.data() + sizeof(fileHandle), &offset, sizeof(offset));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset), &length,
           sizeof(length));

    uint32_t retFileHandle = 0;
    uint32_t retOffset = 0;
    uint32_t retLength = 0;

    // Invoke decode the read file request
    auto rc = decode_read_file_req(requestMsg.data(), requestMsg.size(),
                                   &retFileHandle, &retOffset, &retLength);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(fileHandle, retFileHandle);
    ASSERT_EQ(offset, retOffset);
    ASSERT_EQ(length, retLength);

    // Good decode request test for write file
    std::array<uint8_t, PLDM_CONST_WRITE_FILE_REQ_BYTES + PLDM_MAX_FILE_SIZE>
        requestMsgWr{};

    // Random value for fileHandle, offset, length and file data
    std::vector<uint8_t> fileData = {0xA, 0x1, 0x3, 0xF};
    length = fileData.size();

    memcpy(requestMsgWr.data(), &fileHandle, sizeof(fileHandle));
    memcpy(requestMsgWr.data() + sizeof(fileHandle), &offset, sizeof(offset));
    memcpy(requestMsgWr.data() + sizeof(fileHandle) + sizeof(offset), &length,
           sizeof(length));
    memcpy(requestMsgWr.data() + sizeof(fileHandle) + sizeof(offset) +
               sizeof(length),
           fileData.data(), length);

    retFileHandle = 0;
    retOffset = 0;
    retLength = 0;
    std::vector<uint8_t> retfileData = {0};

    // Invoke decode the write file request
    rc = decode_write_file_req(requestMsgWr.data(), requestMsgWr.size(),
                               &retFileHandle, &retOffset, &retLength,
                               retfileData.data());

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(fileHandle, retFileHandle);
    ASSERT_EQ(offset, retOffset);
    ASSERT_EQ(length, retLength);
    ASSERT_EQ(0, memcmp(fileData.data(), retfileData.data(), length));
}

TEST(ReadWriteFile, testBadDecodeRequest)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;

    // Bad decode request for read file
    std::array<uint8_t, PLDM_READ_FILE_REQ_BYTES> requestMsg{};

    // Request payload message is missing
    auto rc = decode_read_file_req(NULL, 0, &fileHandle, &offset, &length);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Payload length is invalid
    rc = decode_read_file_req(requestMsg.data(), 0, &fileHandle, &offset,
                              &length);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    // Bad decode request for write file
    std::array<uint8_t, PLDM_CONST_WRITE_FILE_REQ_BYTES + PLDM_MAX_FILE_SIZE>
        requestMsgWr{};

    // Request payload message is missing
    std::vector<uint8_t> fileData = {0};

    rc = decode_write_file_req(NULL, 0, &fileHandle, &offset, &length,
                               fileData.data());
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Payload length is invalid
    rc = decode_write_file_req(requestMsgWr.data(), 0, &fileHandle, &offset,
                               &length, fileData.data());
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(ReadWriteFile, testGoodEncodeResponse)
{
    // Good encode request for read file
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_CONST_READ_FILE_RESP_BYTES +
                            PLDM_MAX_FILE_SIZE>
        responseMsg{};

    std::vector<uint8_t> fileData = {0xA, 0x1, 0x3, 0xF};
    uint32_t length = fileData.size();
    pldm_msg* response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    // ReadFile
    auto rc = encode_read_file_resp(0, PLDM_READ_FILE, PLDM_SUCCESS, length,
                                    fileData.data(), response);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(response->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(response->hdr.instance_id, 0);
    ASSERT_EQ(response->hdr.type, PLDM_IBM_OEM_TYPE);
    ASSERT_EQ(response->hdr.command, PLDM_READ_FILE);
    ASSERT_EQ(response->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]),
                        &length, sizeof(length)));
    ASSERT_EQ(0, memcmp((response->payload + sizeof(response->payload[0]) +
                         sizeof(length)),
                        fileData.data(), length));

    // Good encode request for write file
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_WRITE_FILE_RESP_BYTES>
        responseMsgWr{};

    pldm_msg* responseWr = reinterpret_cast<pldm_msg*>(responseMsgWr.data());

    // WriteFile
    rc = encode_write_file_resp(0, PLDM_WRITE_FILE_RESP_BYTES, PLDM_SUCCESS,
                                length, responseWr);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(responseWr->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(responseWr->hdr.instance_id, 0);
    ASSERT_EQ(responseWr->hdr.type, PLDM_IBM_OEM_TYPE);
    ASSERT_EQ(responseWr->hdr.command, PLDM_WRITE_FILE);
    ASSERT_EQ(responseWr->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(responseWr->payload + sizeof(response->payload[0]),
                        &length, sizeof(length)));
}

TEST(ReadWriteFile, testBadEncodeResponse)
{
    // Bad encode request for read file
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_CONST_READ_FILE_RESP_BYTES +
                            PLDM_MAX_FILE_SIZE>
        responseMsg{};
    uint32_t length = 0;
    std::vector<uint8_t> fileData = {0};
    pldm_msg* response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    // ReadFile
    auto rc = encode_read_file_resp(0, PLDM_READ_FILE, PLDM_ERROR, length,
                                    fileData.data(), response);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(response->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(response->hdr.instance_id, 0);
    ASSERT_EQ(response->hdr.type, PLDM_IBM_OEM_TYPE);
    ASSERT_EQ(response->hdr.command, PLDM_READ_FILE);
    ASSERT_EQ(response->payload[0], PLDM_ERROR);

    // Bad encode request for write file
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_WRITE_FILE_RESP_BYTES>
        responseMsgWr{};
    pldm_msg* responseWr = reinterpret_cast<pldm_msg*>(responseMsgWr.data());

    // WriteFile
    rc = encode_write_file_resp(0, PLDM_WRITE_FILE, PLDM_ERROR, length,
                                responseWr);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(responseWr->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(responseWr->hdr.instance_id, 0);
    ASSERT_EQ(responseWr->hdr.type, PLDM_IBM_OEM_TYPE);
    ASSERT_EQ(responseWr->hdr.command, PLDM_WRITE_FILE);
    ASSERT_EQ(responseWr->payload[0], PLDM_ERROR);
}
