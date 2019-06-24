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
    ASSERT_EQ(response->hdr.type, PLDM_OEM);
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
    ASSERT_EQ(response->hdr.type, PLDM_OEM);
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
    ASSERT_EQ(response->hdr.type, PLDM_OEM);
    ASSERT_EQ(response->hdr.command, PLDM_READ_FILE_INTO_MEMORY);
    ASSERT_EQ(response->payload[0], PLDM_ERROR);

    // WriteFileFromMemory
    rc = encode_rw_file_memory_resp(0, PLDM_WRITE_FILE_FROM_MEMORY, PLDM_ERROR,
                                    length, response);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(response->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(response->hdr.instance_id, 0);
    ASSERT_EQ(response->hdr.type, PLDM_OEM);
    ASSERT_EQ(response->hdr.command, PLDM_WRITE_FILE_FROM_MEMORY);
    ASSERT_EQ(response->payload[0], PLDM_ERROR);
}

TEST(ReadWriteFileIntoMemory, testGoodDecodeResponse)
{
    std::array<uint8_t, PLDM_RW_FILE_MEM_RESP_BYTES> responseMsg{};
    // Random value for length
    uint32_t length = 0xFF00EE12;
    uint8_t completionCode = 0;

    memcpy(responseMsg.data(), &completionCode, sizeof(completionCode));
    memcpy(responseMsg.data() + sizeof(completionCode), &length,
           sizeof(length));

    uint8_t retCompletionCode = 0;
    uint32_t retLength = 0;

    // Invoke decode the read file memory response
    auto rc = decode_rw_file_memory_resp(responseMsg.data(), responseMsg.size(),
                                         &retCompletionCode, &retLength);
    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completionCode, retCompletionCode);
    ASSERT_EQ(length, retLength);
}

TEST(ReadWriteFileIntoMemory, testBadDecodeResponse)
{
    uint32_t length = 0;
    uint8_t completionCode = 0;

    // Request payload message is missing
    auto rc = decode_rw_file_memory_resp(NULL, 0, &completionCode, &length);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    std::array<uint8_t, PLDM_RW_FILE_MEM_RESP_BYTES> responseMsg{};

    // Payload length is invalid
    rc = decode_rw_file_memory_resp(responseMsg.data(), 0, &completionCode,
                                    &length);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(ReadWriteFileIntoMemory, testGoodEncodeRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_RW_FILE_MEM_REQ_BYTES>
        requestMsg{};

    uint32_t fileHandle = 0x12345678;
    uint32_t offset = 0x87654321;
    uint32_t length = 0x13245768;
    uint64_t address = 0x124356879ACBDE0F;

    pldm_msg* request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc =
        encode_rw_file_memory_req(0, PLDM_READ_FILE_INTO_MEMORY, fileHandle,
                                  offset, length, address, request);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(request->hdr.request, PLDM_REQUEST);
    ASSERT_EQ(request->hdr.instance_id, 0);
    ASSERT_EQ(request->hdr.type, PLDM_OEM);
    ASSERT_EQ(request->hdr.command, PLDM_READ_FILE_INTO_MEMORY);

    ASSERT_EQ(0, memcmp(request->payload, &fileHandle, sizeof(fileHandle)));

    ASSERT_EQ(0, memcmp(request->payload + sizeof(fileHandle), &offset,
                        sizeof(offset)));
    ASSERT_EQ(0, memcmp(request->payload + sizeof(fileHandle) + sizeof(offset),
                        &length, sizeof(length)));
    ASSERT_EQ(0, memcmp(request->payload + sizeof(fileHandle) + sizeof(offset) +
                            sizeof(length),
                        &address, sizeof(address)));
}

TEST(ReadWriteFileIntoMemory, testBadEncodeRequest)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint64_t address = 0;

    auto rc =
        encode_rw_file_memory_req(0, PLDM_READ_FILE_INTO_MEMORY, fileHandle,
                                  offset, length, address, NULL);

    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(GetFileTable, GoodDecodeRequest)
{
    std::array<uint8_t, PLDM_GET_FILE_TABLE_REQ_BYTES> requestMsg{};

    // Random value for DataTransferHandle, TransferOperationFlag, TableType
    uint32_t transferHandle = 0x12345678;
    uint8_t transferOpFlag = 1;
    uint8_t tableType = 1;

    memcpy(requestMsg.data(), &transferHandle, sizeof(transferHandle));
    memcpy(requestMsg.data() + sizeof(transferHandle), &transferOpFlag,
           sizeof(transferOpFlag));
    memcpy(requestMsg.data() + sizeof(transferHandle) + sizeof(transferOpFlag),
           &tableType, sizeof(tableType));

    uint32_t retTransferHandle = 0;
    uint8_t retTransferOpFlag = 0;
    uint8_t retTableType = 0;

    // Invoke decode get file table request
    auto rc = decode_get_file_table_req(requestMsg.data(), requestMsg.size(),
                                        &retTransferHandle, &retTransferOpFlag,
                                        &retTableType);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(transferHandle, retTransferHandle);
    ASSERT_EQ(transferOpFlag, retTransferOpFlag);
    ASSERT_EQ(tableType, retTableType);
}

TEST(GetFileTable, BadDecodeRequest)
{
    uint32_t transferHandle = 0;
    uint8_t transferOpFlag = 0;
    uint8_t tableType = 0;

    // Request payload message is missing
    auto rc = decode_get_file_table_req(nullptr, 0, &transferHandle,
                                        &transferOpFlag, &tableType);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    std::array<uint8_t, PLDM_GET_FILE_TABLE_REQ_BYTES> requestMsg{};

    // TableType is NULL
    rc = decode_get_file_table_req(requestMsg.data(), requestMsg.size(),
                                   &transferHandle, &transferOpFlag, nullptr);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Payload length is invalid
    rc = decode_get_file_table_req(requestMsg.data(), 0, &transferHandle,
                                   &transferOpFlag, &tableType);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(GetFileTable, GoodEncodeResponse)
{
    // Random value for NextDataTransferHandle and TransferFlag
    uint8_t completionCode = 0;
    uint32_t nextTransferHandle = 0x87654321;
    uint8_t transferFlag = 5;
    // Mock file table contents of size 5
    std::array<uint8_t, 5> fileTable = {1, 2, 3, 4, 5};
    constexpr size_t responseSize = sizeof(completionCode) +
                                    sizeof(nextTransferHandle) +
                                    sizeof(transferFlag) + fileTable.size();

    std::array<uint8_t, sizeof(pldm_msg_hdr) + responseSize> responseMsg{};
    pldm_msg* response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    // GetFileTable
    auto rc = encode_get_file_table_resp(0, PLDM_SUCCESS, nextTransferHandle,
                                         transferFlag, fileTable.data(),
                                         fileTable.size(), response);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(response->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(response->hdr.instance_id, 0);
    ASSERT_EQ(response->hdr.type, PLDM_OEM);
    ASSERT_EQ(response->hdr.command, PLDM_GET_FILE_TABLE);
    ASSERT_EQ(response->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]),
                        &nextTransferHandle, sizeof(nextTransferHandle)));
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]) +
                            sizeof(nextTransferHandle),
                        &transferFlag, sizeof(transferFlag)));
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]) +
                            sizeof(nextTransferHandle),
                        &transferFlag, sizeof(transferFlag)));
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]) +
                            sizeof(nextTransferHandle) + sizeof(transferFlag),
                        fileTable.data(), fileTable.size()));
}

TEST(GetFileTable, BadEncodeResponse)
{
    uint8_t completionCode = 0;
    uint32_t nextTransferHandle = 0;
    uint8_t transferFlag = 0;
    constexpr size_t responseSize = sizeof(completionCode) +
                                    sizeof(nextTransferHandle) +
                                    sizeof(transferFlag);

    std::array<uint8_t, sizeof(pldm_msg_hdr) + responseSize> responseMsg{};
    pldm_msg* response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    // GetFileTable
    auto rc = encode_get_file_table_resp(0, PLDM_ERROR, nextTransferHandle,
                                         transferFlag, nullptr, 0, response);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(response->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(response->hdr.instance_id, 0);
    ASSERT_EQ(response->hdr.type, PLDM_OEM);
    ASSERT_EQ(response->hdr.command, PLDM_GET_FILE_TABLE);
    ASSERT_EQ(response->payload[0], PLDM_ERROR);
}

TEST(ReadFile, testGoodDecodeRequest)
{
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
}

TEST(WriteFile, testGoodDecodeRequest)
{
    std::array<uint8_t, PLDM_CONST_WRITE_FILE_REQ_BYTES + PLDM_MAX_FILE_SIZE>
        requestMsg{};

    // Random value for fileHandle, offset, length and file data
    uint32_t fileHandle = 0x12345678;
    uint32_t offset = 0x87654321;
    std::vector<uint8_t> fileData = {0xA, 0x1, 0x3, 0xF};
    uint32_t length = fileData.size();

    memcpy(requestMsg.data(), &fileHandle, sizeof(fileHandle));
    memcpy(requestMsg.data() + sizeof(fileHandle), &offset, sizeof(offset));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset), &length,
           sizeof(length));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset) +
               sizeof(length),
           fileData.data(), length);

    uint32_t retFileHandle = 0;
    uint32_t retOffset = 0;
    uint32_t retLength = 0;
    std::vector<uint8_t> retfileData = {0};

    // Invoke decode the write file request
    auto rc = decode_write_file_req(requestMsg.data(), requestMsg.size(),
                                    &retFileHandle, &retOffset, &retLength,
                                    retfileData.data());

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(fileHandle, retFileHandle);
    ASSERT_EQ(offset, retOffset);
    ASSERT_EQ(length, retLength);
    ASSERT_EQ(0, memcmp(fileData.data(), retfileData.data(), length));
}

TEST(ReadWriteFile, testGoodDecodeResponse)
{
    std::array<uint8_t, PLDM_CONST_READ_FILE_RESP_BYTES + PLDM_MAX_FILE_SIZE>
        responseMsg{};

    // Random value for file data and length
    std::vector<uint8_t> fileData = {0xA, 0x1, 0x3, 0xF};
    uint32_t length = fileData.size();
    uint8_t completionCode = 0x4;

    memcpy(responseMsg.data(), &completionCode, sizeof(completionCode));
    memcpy(responseMsg.data() + sizeof(completionCode), &length,
           sizeof(length));
    memcpy(responseMsg.data() + sizeof(completionCode) + sizeof(length),
           fileData.data(), length);

    uint32_t retLength = 0;
    uint8_t retCompletionCode = 0;
    std::vector<uint8_t> retfileData = {0};

    // Invoke decode the read file response
    auto rc = decode_read_file_resp(responseMsg.data(), responseMsg.size(),
                                    &retCompletionCode, &retLength,
                                    retfileData.data());

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completionCode, retCompletionCode);
    ASSERT_EQ(length, retLength);
    ASSERT_EQ(0, memcmp(fileData.data(), retfileData.data(), length));

    std::array<uint8_t, PLDM_WRITE_FILE_RESP_BYTES> responseMsgWr{};

    memcpy(responseMsgWr.data(), &completionCode, sizeof(completionCode));
    memcpy(responseMsgWr.data() + sizeof(completionCode), &length,
           sizeof(length));

    retLength = 0;
    retCompletionCode = 0;

    // Invoke decode the write file response
    rc = decode_write_file_resp(responseMsgWr.data(), responseMsgWr.size(),
                                &retCompletionCode, &retLength);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(completionCode, retCompletionCode);
    ASSERT_EQ(length, retLength);
}

TEST(ReadWriteFile, testBadDecodeResponse)
{
    std::vector<uint8_t> fileData = {0};
    uint32_t length = 0;
    uint8_t completionCode = 0;

    // Bad decode response for read file
    std::array<uint8_t, PLDM_CONST_READ_FILE_RESP_BYTES + PLDM_MAX_FILE_SIZE>
        requestMsg{};

    // Request payload message is missing
    auto rc = decode_read_file_resp(NULL, 0, &completionCode, &length,
                                    fileData.data());
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Payload length is invalid
    rc = decode_read_file_resp(requestMsg.data(), 0, &completionCode, &length,
                               fileData.data());
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    // Bad decode request for write file
    std::array<uint8_t, PLDM_WRITE_FILE_RESP_BYTES> requestMsgWr{};

    // Request payload message is missing
    rc = decode_write_file_resp(NULL, 0, &completionCode, &length);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Payload length is invalid
    rc = decode_write_file_resp(requestMsgWr.data(), 0, &completionCode,
                                &length);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
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
    // Good encode response for read file
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_CONST_READ_FILE_RESP_BYTES +
                            PLDM_MAX_FILE_SIZE>
        responseMsg{};

    std::vector<uint8_t> fileData = {0xA, 0x1, 0x3, 0xF};
    uint32_t length = fileData.size();
    pldm_msg* response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    // ReadFile
    auto rc = encode_read_file_resp(0, PLDM_SUCCESS, length, fileData.data(),
                                    response);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(response->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(response->hdr.instance_id, 0);
    ASSERT_EQ(response->hdr.type, PLDM_OEM);
    ASSERT_EQ(response->hdr.command, PLDM_READ_FILE);
    ASSERT_EQ(response->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]),
                        &length, sizeof(length)));
    ASSERT_EQ(0, memcmp((response->payload + sizeof(response->payload[0]) +
                         sizeof(length)),
                        fileData.data(), length));

    // Good encode response for write file
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_WRITE_FILE_RESP_BYTES>
        responseMsgWr{};

    pldm_msg* responseWr = reinterpret_cast<pldm_msg*>(responseMsgWr.data());

    // WriteFile
    rc = encode_write_file_resp(0, PLDM_SUCCESS, length, responseWr);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(responseWr->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(responseWr->hdr.instance_id, 0);
    ASSERT_EQ(responseWr->hdr.type, PLDM_OEM);
    ASSERT_EQ(responseWr->hdr.command, PLDM_WRITE_FILE);
    ASSERT_EQ(responseWr->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(responseWr->payload + sizeof(response->payload[0]),
                        &length, sizeof(length)));
}

TEST(ReadWriteFile, testGoodEncodeRequest)
{
    // Good encode request for read file
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_READ_FILE_REQ_BYTES>
        requestMsg{};

    uint32_t fileHandle = 0x12345678;
    uint32_t offset = 0x87654321;
    uint32_t length = 0x13245768;
    pldm_msg* request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    // ReadFile
    auto rc = encode_read_file_req(0, fileHandle, offset, length, request);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(request->hdr.request, PLDM_REQUEST);
    ASSERT_EQ(request->hdr.instance_id, 0);
    ASSERT_EQ(request->hdr.type, PLDM_OEM);
    ASSERT_EQ(request->hdr.command, PLDM_READ_FILE);
    ASSERT_EQ(0, memcmp(request->payload, &fileHandle, sizeof(fileHandle)));
    ASSERT_EQ(0, memcmp(request->payload + sizeof(fileHandle), &offset,
                        sizeof(offset)));
    ASSERT_EQ(0,
              memcmp((request->payload + sizeof(fileHandle) + sizeof(offset)),
                     &length, sizeof(length)));

    // Good encode request for write file
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_CONST_WRITE_FILE_REQ_BYTES +
                            PLDM_MAX_FILE_SIZE>
        requestMsgWr{};

    std::vector<uint8_t> fileData = {0xA, 0x1, 0x3, 0xF};
    length = fileData.size();
    pldm_msg* requestWr = reinterpret_cast<pldm_msg*>(requestMsgWr.data());

    // WriteFile
    rc = encode_write_file_req(0, fileHandle, offset, length, fileData.data(),
                               requestWr);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(requestWr->hdr.request, PLDM_REQUEST);
    ASSERT_EQ(requestWr->hdr.instance_id, 0);
    ASSERT_EQ(requestWr->hdr.type, PLDM_OEM);
    ASSERT_EQ(requestWr->hdr.command, PLDM_WRITE_FILE);
    ASSERT_EQ(0, memcmp(requestWr->payload, &fileHandle, sizeof(fileHandle)));
    ASSERT_EQ(0, memcmp(requestWr->payload + sizeof(fileHandle), &offset,
                        sizeof(offset)));
    ASSERT_EQ(0,
              memcmp(requestWr->payload + sizeof(fileHandle) + sizeof(offset),
                     &length, sizeof(length)));
    ASSERT_EQ(0, memcmp(requestWr->payload + sizeof(fileHandle) +
                            sizeof(offset) + sizeof(length),
                        fileData.data(), length));
}

TEST(ReadWriteFile, testBadEncodeRequest)
{
    // Bad encode request for read file
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_READ_FILE_REQ_BYTES>
        requestMsg{};
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0x13456789;
    pldm_msg* request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    // ReadFile check invalid file handle
    auto rc = encode_read_file_req(0, fileHandle, offset, length, request);

    ASSERT_EQ(rc, PLDM_INVALID_FILE_HANDLE);

    length = 0;
    fileHandle = 0x13456789;

    // ReadFile check invalid file length
    rc = encode_read_file_req(0, fileHandle, offset, length, request);

    ASSERT_EQ(rc, PLDM_INVALID_READ_LENGTH);

    // Bad encode request for write file
    std::vector<uint8_t> fileData = {0};
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_CONST_WRITE_FILE_REQ_BYTES +
                            PLDM_MAX_FILE_SIZE>
        requestMsgWr{};
    pldm_msg* requestWr = reinterpret_cast<pldm_msg*>(requestMsgWr.data());

    // WriteFile check for invalid file handle
    length = 0x13456789;
    fileHandle = 0;
    rc = encode_write_file_req(0, fileHandle, offset, length, fileData.data(),
                               requestWr);

    ASSERT_EQ(rc, PLDM_INVALID_FILE_HANDLE);

    // WriteFile check for invalid file length
    length = 0;
    fileHandle = 0x13456789;
    rc = encode_write_file_req(0, fileHandle, offset, length, fileData.data(),
                               requestWr);

    ASSERT_EQ(rc, PLDM_INVALID_WRITE_LENGTH);
}

TEST(ReadWriteFile, testBadEncodeResponse)
{
    // Bad encode response for read file
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_CONST_READ_FILE_RESP_BYTES +
                            PLDM_MAX_FILE_SIZE>
        responseMsg{};
    uint32_t length = 0;
    std::vector<uint8_t> fileData = {0};
    pldm_msg* response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    // ReadFile
    auto rc =
        encode_read_file_resp(0, PLDM_ERROR, length, fileData.data(), response);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(response->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(response->hdr.instance_id, 0);
    ASSERT_EQ(response->hdr.type, PLDM_OEM);
    ASSERT_EQ(response->hdr.command, PLDM_READ_FILE);
    ASSERT_EQ(response->payload[0], PLDM_ERROR);

    // Bad encode response for write file
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_WRITE_FILE_RESP_BYTES>
        responseMsgWr{};
    pldm_msg* responseWr = reinterpret_cast<pldm_msg*>(responseMsgWr.data());

    // WriteFile
    rc = encode_write_file_resp(0, PLDM_ERROR, length, responseWr);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(responseWr->hdr.request, PLDM_RESPONSE);
    ASSERT_EQ(responseWr->hdr.instance_id, 0);
    ASSERT_EQ(responseWr->hdr.type, PLDM_OEM);
    ASSERT_EQ(responseWr->hdr.command, PLDM_WRITE_FILE);
    ASSERT_EQ(responseWr->payload[0], PLDM_ERROR);
}
