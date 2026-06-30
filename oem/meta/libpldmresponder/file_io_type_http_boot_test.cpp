#include "file_io_type_http_boot.hpp"

#include <unistd.h>

#include <array>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace pldm::responder::oem_meta
{
namespace
{
class HttpBootFileIOTest : public testing::Test
{
  protected:
    void SetUp() override
    {
        std::array<char, 32> pathTemplate{"/tmp/pldm-http-boot.XXXXXX"};
        fd = mkstemp(pathTemplate.data());
        ASSERT_NE(fd, -1);
        path = pathTemplate.data();
        constexpr std::array<uint8_t, 4> contents{1, 2, 3, 4};
        ASSERT_EQ(::write(fd, contents.data(), contents.size()),
                  static_cast<ssize_t>(contents.size()));
        ASSERT_EQ(close(fd), 0);
        fd = -1;
    }

    void TearDown() override
    {
        if (fd >= 0)
        {
            close(fd);
        }
        unlink(path.c_str());
    }

    static pldm_oem_meta_file_io_read_resp* makeResponse(
        std::vector<uint8_t>& storage, uint8_t length, uint16_t offset)
    {
        storage.resize(sizeof(pldm_oem_meta_file_io_read_resp) + length);
        auto* response = new (storage.data()) pldm_oem_meta_file_io_read_resp{};
        response->option = PLDM_OEM_META_FILE_IO_READ_DATA;
        response->length = length;
        response->info.data.offset = offset;
        return response;
    }

    int fd = -1;
    std::string path;
};

TEST_F(HttpBootFileIOTest, RejectsOffsetPastEndOfFile)
{
    std::vector<uint8_t> storage;
    auto* response = makeResponse(storage, 1, 5);
    HttpBootHandler handler(path);

    EXPECT_EQ(handler.read(response), PLDM_ERROR_INVALID_DATA);
    EXPECT_EQ(response->length, 0);
}

TEST_F(HttpBootFileIOTest, BoundsReadToCallerCapacityAndFileSize)
{
    std::vector<uint8_t> storage;
    auto* response = makeResponse(storage, 4, 3);
    HttpBootHandler handler(path);

    ASSERT_EQ(handler.read(response), PLDM_SUCCESS);
    EXPECT_EQ(response->length, 1);
    EXPECT_EQ(response->info.data.offset, 4);
    EXPECT_EQ(response->info.data.transferFlag, PLDM_END);
    EXPECT_EQ(
        *static_cast<uint8_t*>(pldm_oem_meta_file_io_read_resp_data(response)),
        4);
}
} // namespace
} // namespace pldm::responder::oem_meta
