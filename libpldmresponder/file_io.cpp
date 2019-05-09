#include "file_io.hpp"

#include "registration.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <phosphor-logging/log.hpp>

#include "libpldm/base.h"

namespace pldm
{

namespace responder
{

namespace fileio
{

void registerHandlers()
{
    registerHandler(PLDM_FILE_IO, PLDM_READ_FILE_INTO_MEMORY,
                    std::move(readFileIntoMemory));
    registerHandler(PLDM_FILE_IO, PLDM_WRITE_FILE_FROM_MEMORY,
                    std::move(writeFileFromMemory));
}

namespace fs = std::filesystem;
using namespace phosphor::logging;

namespace dma
{

/** @struct AspeedXdmaOp
 *
 * Structure representing XDMA operation
 */
struct AspeedXdmaOp
{
    uint8_t upstream;  //!< boolean indicating the direction of the DMA
                       //!< operation, true means a transfer from BMC to host.
    uint64_t hostAddr; //!< the DMA address on the host side, configured by
                       //!< PCI subsystem.
    uint32_t len;      //!< the size of the transfer in bytes, it should be a
                       //!< multiple of 16 bytes
} __attribute__((packed));

constexpr auto xdmaDev = "/dev/xdma";

int DMA::transferDataHost(const fs::path& path, uint32_t offset,
                          uint32_t length, uint64_t address, bool upstream)
{
    static const size_t pageSize = getpagesize();
    uint32_t numPages = length / pageSize;
    uint32_t pageAlignedLength = numPages * pageSize;

    if (length > pageAlignedLength)
    {
        pageAlignedLength += pageSize;
    }

    auto mmapCleanup = [pageAlignedLength](void* vgaMem) {
        munmap(vgaMem, pageAlignedLength);
    };

    int fd = -1;
    int rc = 0;
    fd = open(xdmaDev, O_RDWR);
    if (fd < 0)
    {
        rc = -errno;
        log<level::ERR>("Failed to open the XDMA device", entry("RC=%d", rc));
        return rc;
    }

    utils::CustomFD xdmaFd(fd);

    void* vgaMem;
    vgaMem = mmap(nullptr, pageAlignedLength, upstream ? PROT_WRITE : PROT_READ,
                  MAP_SHARED, xdmaFd(), 0);
    if (MAP_FAILED == vgaMem)
    {
        rc = -errno;
        log<level::ERR>("Failed to mmap the XDMA device", entry("RC=%d", rc));
        return rc;
    }

    std::unique_ptr<void, decltype(mmapCleanup)> vgaMemPtr(vgaMem, mmapCleanup);

    if (upstream)
    {
        std::ifstream stream(path.string());

        stream.seekg(offset);
        stream.read(static_cast<char*>(vgaMemPtr.get()), length);
    }

    AspeedXdmaOp xdmaOp;
    xdmaOp.upstream = upstream ? 1 : 0;
    xdmaOp.hostAddr = address;
    xdmaOp.len = length;

    rc = write(xdmaFd(), &xdmaOp, sizeof(xdmaOp));
    if (rc < 0)
    {
        rc = -errno;

        log<level::ERR>("Failed to execute the DMA operation",
                        entry("RC=%d", rc));
        return rc;
    }

    if (!upstream)
    {
        std::ofstream stream(path.string());

        stream.seekp(offset);
        stream.write(static_cast<const char*>(vgaMemPtr.get()), length);
    }

    return 0;
}

} // namespace dma

void readFileIntoMemory(const pldm_msg_payload* request, pldm_msg* response)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint64_t address = 0;

    if (request->payload_length != (sizeof(fileHandle) + sizeof(offset) +
                                    sizeof(length) + sizeof(address)))
    {
        encode_rw_file_memory_resp(0, PLDM_READ_FILE_INTO_MEMORY,
                                   PLDM_ERROR_INVALID_LENGTH, 0, response);
        return;
    }

    decode_rw_file_memory_req(request, &fileHandle, &offset, &length, &address);

    if (length % dma::minSize)
    {
        log<level::ERR>("Readlength is not a multiple of 16");
        encode_rw_file_memory_resp(0, PLDM_READ_FILE_INTO_MEMORY,
                                   PLDM_INVALID_READ_LENGTH, 0, response);
        return;
    }

    std::string readFilePath;
    if (fileHandle == 0)
    {
        readFilePath = "/var/lib/pldm/PHYP-NVRAM";
    }
    else if (fileHandle == 1)
    {
        readFilePath = "/var/lib/pldm/PHYP-NVRAM-CKSUM";
    }
    else
    {
        log<level::ERR>("Invalid file handle");
        encode_rw_file_memory_resp(0, PLDM_READ_FILE_INTO_MEMORY,
                                   PLDM_INVALID_FILE_HANDLE, 0, response);
    }
    fs::path path{readFilePath};

    if (!fs::exists(path))
    {

        log<level::ERR>("File does not exist");
        encode_rw_file_memory_resp(0, PLDM_READ_FILE_INTO_MEMORY,
                                   PLDM_INVALID_FILE_HANDLE, 0, response);
        return;
    }

    auto fileSize = fs::file_size(path);
    if (offset + length > fileSize)
    {
        log<level::ERR>("Data out of range");
        encode_rw_file_memory_resp(0, PLDM_READ_FILE_INTO_MEMORY,
                                   PLDM_DATA_OUT_OF_RANGE, 0, response);
        return;
    }

    using namespace dma;
    DMA intf;
    transferAll<DMA>(&intf, PLDM_READ_FILE_INTO_MEMORY, path, offset, length,
                     address, true, response);
}

void writeFileFromMemory(const pldm_msg_payload* request, pldm_msg* response)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint64_t address = 0;

    if (request->payload_length != (sizeof(fileHandle) + sizeof(offset) +
                                    sizeof(length) + sizeof(address)))
    {
        encode_rw_file_memory_resp(0, PLDM_WRITE_FILE_FROM_MEMORY,
                                   PLDM_ERROR_INVALID_LENGTH, 0, response);
        return;
    }

    decode_rw_file_memory_req(request, &fileHandle, &offset, &length, &address);

    if (length % dma::minSize)
    {
        log<level::ERR>("Writelength is not a multiple of 16");
        encode_rw_file_memory_resp(0, PLDM_WRITE_FILE_FROM_MEMORY,
                                   PLDM_INVALID_WRITE_LENGTH, 0, response);
        return;
    }

    std::string readFilePath;
    if (fileHandle == 0)
    {
        readFilePath = "/var/lib/pldm/PHYP-NVRAM";
    }
    else if (fileHandle == 1)
    {
        readFilePath = "/var/lib/pldm/PHYP-NVRAM-CKSUM";
    }
    else
    {
        log<level::ERR>("Invalid file handle");
        encode_rw_file_memory_resp(0, PLDM_WRITE_FILE_FROM_MEMORY,
                                   PLDM_INVALID_FILE_HANDLE, 0, response);
    }
    fs::path path{readFilePath};

    if (!fs::exists(path))
    {
        log<level::ERR>("File does not exist");
        encode_rw_file_memory_resp(0, PLDM_WRITE_FILE_FROM_MEMORY,
                                   PLDM_INVALID_FILE_HANDLE, 0, response);
        return;
    }

    using namespace dma;
    DMA intf;
    transferAll<DMA>(&intf, PLDM_WRITE_FILE_FROM_MEMORY, path, offset, length,
                     address, false, response);
}

} // namespace fileio
} // namespace responder
} // namespace pldm
