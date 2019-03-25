#include "file_io.hpp"
#include "registration.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

#include "libpldm/base.h"

namespace pldm
{

namespace responder
{

namespace fs = std::filesystem;

namespace fileio
{
void registerHandlers()
{
registerHandler(PLDM_FILE_IO, PLDM_READ_FILE_MEMORY, std::move(readFileIntoMemory));
}


}

struct AspeedXdmaOp
{
    uint8_t upstream;
    uint64_t hostAddr;
    uint32_t len;
} __attribute__((packed));

int dmaTransferData(const fs::path& file, const uint32_t offset,
                    const uint32_t length, const uint64_t address)
{
    const size_t pageSize = getpagesize();
    uint32_t numPages = length / pageSize;
    uint32_t pageLength = numPages * pageSize;
    if (length > pageLength)
        pageLength += pageSize;

    int rc = 0;
    int fd = -1;
    const char* xdmaDev = "/dev/xdma";
    void* vgaMem = nullptr;
    std::ifstream stream(file.string());

    fd = open(xdmaDev, O_RDWR);
    if (fd < 0)
    {
        rc = -ENODEV;
        goto done;
    }

    vgaMem = mmap(NULL, pageLength, PROT_WRITE, MAP_SHARED, fd, 0);
    if (!vgaMem)
    {
        rc = -ENOMEM;
        goto done;
    }

    stream.seekg(offset);
    stream.read(static_cast<char*>(vgaMem), length);

    struct AspeedXdmaOp xdmaOp;
    xdmaOp.upstream = 1;
    xdmaOp.hostAddr = address;
    xdmaOp.len = length;

    rc = write(fd, &xdmaOp, sizeof(xdmaOp));
    if (rc < 0)
    {
        rc = -errno;
    }

done:
    if (vgaMem)
    {
        munmap(vgaMem, pageLength);
    }

    if (fd >= 0)
    {
        close(fd);
    }

    return rc;
}

void readFileIntoMemory(const pldm_msg_payload* request, pldm_msg* response)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint64_t address = 0;

    if (request->payload_length != (sizeof(fileHandle) + sizeof(offset) +
                                    sizeof(length) + sizeof(address)))
    {
        encode_read_file_memory_resp(0, PLDM_ERROR_INVALID_LENGTH, 0, response);
        return;
    }

    decode_read_file_memory_req(request, &fileHandle, &offset, &length,
                                &address);

    // Hardcoding the file name till the GetFileTable infrastructure is in
    // place.
    constexpr auto readFilePath = "/tmp/readfile";
    uint32_t origLength = length;

    fs::path path{readFilePath};
    if (!fs::exists(path))
    {
        std::cerr << "File does not exist"
                  << "\n";
        encode_read_file_memory_resp(0, PLDM_INVALID_FILE_HANDLE, 0, response);
        return;
    }

    constexpr uint32_t minLength = 16;
    if (length < minLength)
    {
        std::cerr << "Readlength is less than 16"
                  << "\n";
        encode_read_file_memory_resp(0, PLDM_INVALID_READ_LENGTH, 0, response);
        return;
    }

    auto fileSize = fs::file_size(path);
    if (offset + length > fileSize)
    {
        std::cerr << "Data out of range"
                  << "\n";
        encode_read_file_memory_resp(0, PLDM_DATA_OUT_OF_RANGE, 0, response);
        return;
    }

    // There is a restriction on the maximum size to 16MB. This should be made
    // into a configurable parameter 16MB - 16777216 bytes.
    constexpr size_t maxDMASize = 16 * 1024 * 1024;

    while (length > 0)
    {
        if (length > maxDMASize)
        {
            std::cerr << "offset = " << offset << "length = " << length
                      << "address = " << address << "\n";
            //            auto rc = dmaTransferData(path, offset, maxDMASize,
            //            address); if (rc < 0)
            //            {
            //                encode_read_file_memory_resp(0, PLDM_ERROR, 0,
            //                response); return;
            //            }
            offset += maxDMASize;
            length -= maxDMASize;
            address += maxDMASize;
            std::cerr << "offset = " << offset << "length = " << length
                      << "address = " << address << "\n";
        }
        else
        {
            //            auto rc = dmaTransferData(path, offset, length,
            //            address); if (rc < 0)
            //            {
            //                encode_read_file_memory_resp(0, PLDM_ERROR, 0,
            //                response); return;
            //            }
            std::cerr << "Last: offset = " << offset << "length = " << length
                      << "address = " << address << "\n";
            encode_read_file_memory_resp(0, PLDM_SUCCESS, origLength, response);
            return;
        }
    }
}

} // namespace responder
} // namespace pldm
