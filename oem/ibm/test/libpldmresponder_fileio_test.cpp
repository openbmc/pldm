#include "libpldmresponder/file_io.hpp"

#include "libpldm/base.h"
#include "libpldm/file_io.h"

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#define SD_JOURNAL_SUPPRESS_LOCATION

#include <systemd/sd-journal.h>

std::vector<std::string> logs;

extern "C" {

int sd_journal_send(const char* format, ...)
{
    logs.push_back(format);
    return 0;
}

int sd_journal_send_with_location(const char* file, const char* line,
                                  const char* func, const char* format, ...)
{
    logs.push_back(format);
    return 0;
}
}

namespace pldm
{

namespace responder
{

namespace dma
{

class MockDMA
{
  public:
    MOCK_METHOD5(transferDataHost,
                 int(const fs::path& file, uint32_t offset, uint32_t length,
                     uint64_t address, bool upstream));
};

} // namespace dma
} // namespace responder
} // namespace pldm
using namespace pldm::responder;
using ::testing::_;
using ::testing::Return;

TEST(TransferDataHost, GoodPath)
{
    using namespace pldm::responder::dma;

    MockDMA dmaObj;
    fs::path path("");

    // Minimum length of 16 and expect transferDataHost to be called once
    // returns the default value of 0 (the return type of transferDataHost is
    // int, the default value for int is 0)
    uint32_t length = minSize;
    EXPECT_CALL(dmaObj, transferDataHost(path, 0, length, 0, true)).Times(1);
    auto response = transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY,
                                         path, 0, length, 0, true);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(responsePtr->payload + sizeof(responsePtr->payload[0]),
                        &length, sizeof(length)));

    // maxsize of DMA
    length = maxSize;
    EXPECT_CALL(dmaObj, transferDataHost(path, 0, length, 0, true)).Times(1);
    response = transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY, path,
                                    0, length, 0, true);
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(responsePtr->payload + sizeof(responsePtr->payload[0]),
                        &length, sizeof(length)));

    // length greater than maxsize of DMA
    length = maxSize + minSize;
    EXPECT_CALL(dmaObj, transferDataHost(path, 0, maxSize, 0, true)).Times(1);
    EXPECT_CALL(dmaObj, transferDataHost(path, maxSize, minSize, maxSize, true))
        .Times(1);
    response = transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY, path,
                                    0, length, 0, true);
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(responsePtr->payload + sizeof(responsePtr->payload[0]),
                        &length, sizeof(length)));

    // length greater than 2*maxsize of DMA
    length = 3 * maxSize;
    EXPECT_CALL(dmaObj, transferDataHost(_, _, _, _, true)).Times(3);
    response = transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY, path,
                                    0, length, 0, true);
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(responsePtr->payload + sizeof(responsePtr->payload[0]),
                        &length, sizeof(length)));

    // check for downstream(copy data from host to BMC) parameter
    length = minSize;
    EXPECT_CALL(dmaObj, transferDataHost(path, 0, length, 0, false)).Times(1);
    response = transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY, path,
                                    0, length, 0, false);
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(responsePtr->payload + sizeof(responsePtr->payload[0]),
                        &length, sizeof(length)));
}

TEST(TransferDataHost, BadPath)
{
    using namespace pldm::responder::dma;

    MockDMA dmaObj;
    fs::path path("");

    // Minimum length of 16 and transferDataHost returning a negative errno
    uint32_t length = minSize;
    EXPECT_CALL(dmaObj, transferDataHost(_, _, _, _, _)).WillOnce(Return(-1));
    auto response = transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY,
                                         path, 0, length, 0, true);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_ERROR);

    // length greater than maxsize of DMA and transferDataHost returning a
    // negative errno
    length = maxSize + minSize;
    EXPECT_CALL(dmaObj, transferDataHost(_, _, _, _, _)).WillOnce(Return(-1));
    response = transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY, path,
                                    0, length, 0, true);
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_ERROR);
}

TEST(ReadFileIntoMemory, BadPath)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 10;
    uint64_t address = 0;

    std::array<uint8_t, PLDM_RW_FILE_MEM_REQ_BYTES> requestMsg{};
    memcpy(requestMsg.data(), &fileHandle, sizeof(fileHandle));
    memcpy(requestMsg.data() + sizeof(fileHandle), &offset, sizeof(offset));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset), &length,
           sizeof(length));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset) +
               sizeof(length),
           &address, sizeof(address));

    // Pass invalid payload length
    auto response = readFileIntoMemory(requestMsg.data(), 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_ERROR_INVALID_LENGTH);
}

TEST(WriteFileFromMemory, BadPath)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 10;
    uint64_t address = 0;

    std::array<uint8_t, PLDM_RW_FILE_MEM_REQ_BYTES> requestMsg{};
    memcpy(requestMsg.data(), &fileHandle, sizeof(fileHandle));
    memcpy(requestMsg.data() + sizeof(fileHandle), &offset, sizeof(offset));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset), &length,
           sizeof(length));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset) +
               sizeof(length),
           &address, sizeof(address));

    // Pass invalid payload length
    auto response = writeFileFromMemory(requestMsg.data(), 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_ERROR_INVALID_LENGTH);

    // The length field is not a multiple of DMA minsize
    response = writeFileFromMemory(requestMsg.data(), requestMsg.size());
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_INVALID_WRITE_LENGTH);
}
