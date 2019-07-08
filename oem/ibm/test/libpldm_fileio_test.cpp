#include <string.h>

#include <array>

#include "libpldm/base.h"
#include "libpldm/file_io.h"

#include <gtest/gtest.h>

constexpr auto hdrSize = sizeof(pldm_msg_hdr);

TEST(ReadWriteFileMemory, testGoodDecodeRequest)
{
    std::array<uint8_t, PLDM_RW_FILE_MEM_REQ_BYTES + hdrSize> requestMsg{};

    // Random value for fileHandle, offset, length, address
    uint32_t fileHandle = 0x12345678;
    uint32_t offset = 0x87654321;
    uint32_t length = 0x13245768;
    uint64_t address = 0x124356879ACBDE0F;

    memcpy(requestMsg.data() + hdrSize, &fileHandle, sizeof(fileHandle));
    memcpy(requestMsg.data() + sizeof(fileHandle) + hdrSize, &offset,
           sizeof(offset));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset) + hdrSize,
           &length, sizeof(length));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset) +
               sizeof(length) + hdrSize,
           &address, sizeof(address));

    uint32_t retFileHandle = 0;
    uint32_t retOffset = 0;
    uint32_t retLength = 0;
    uint64_t retAddress = 0;

    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    // Invoke decode the read file memory request
    auto rc = decode_rw_file_memory_req(request, requestMsg.size() - hdrSize,
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

    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    // Address is NULL
    rc = decode_rw_file_memory_req(request, requestMsg.size() - hdrSize,
                                   &fileHandle, &offset, &length, NULL);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Payload length is invalid
    rc = decode_rw_file_memory_req(request, 0, &fileHandle, &offset, &length,
                                   &address);
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
    std::array<uint8_t, PLDM_RW_FILE_MEM_RESP_BYTES + hdrSize> responseMsg{};
    // Random value for length
    uint32_t length = 0xFF00EE12;
    uint8_t completionCode = 0;

    memcpy(responseMsg.data() + hdrSize, &completionCode,
           sizeof(completionCode));
    memcpy(responseMsg.data() + sizeof(completionCode) + hdrSize, &length,
           sizeof(length));

    uint8_t retCompletionCode = 0;
    uint32_t retLength = 0;

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    // Invoke decode the read file memory response
    auto rc = decode_rw_file_memory_resp(response, responseMsg.size() - hdrSize,
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

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    // Payload length is invalid
    rc = decode_rw_file_memory_resp(response, 0, &completionCode, &length);
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
    std::array<uint8_t, PLDM_GET_FILE_TABLE_REQ_BYTES + hdrSize> requestMsg{};

    // Random value for DataTransferHandle, TransferOperationFlag, TableType
    uint32_t transferHandle = 0x12345678;
    uint8_t transferOpFlag = 1;
    uint8_t tableType = 1;

    memcpy(requestMsg.data() + hdrSize, &transferHandle,
           sizeof(transferHandle));
    memcpy(requestMsg.data() + sizeof(transferHandle) + hdrSize,
           &transferOpFlag, sizeof(transferOpFlag));
    memcpy(requestMsg.data() + sizeof(transferHandle) + sizeof(transferOpFlag) +
               hdrSize,
           &tableType, sizeof(tableType));

    uint32_t retTransferHandle = 0;
    uint8_t retTransferOpFlag = 0;
    uint8_t retTableType = 0;

    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    // Invoke decode get file table request
    auto rc = decode_get_file_table_req(request, requestMsg.size() - hdrSize,
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

    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    // TableType is NULL
    rc = decode_get_file_table_req(request, requestMsg.size() - hdrSize,
                                   &transferHandle, &transferOpFlag, nullptr);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Payload length is invalid
    rc = decode_get_file_table_req(request, 0, &transferHandle, &transferOpFlag,
                                   &tableType);
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
