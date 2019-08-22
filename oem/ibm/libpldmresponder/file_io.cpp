#include "config.h"

#include "file_io.hpp"

#include "file_io_by_type.hpp"
#include "file_table.hpp"
#include "libpldmresponder/utils.hpp"
#include "registration.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <phosphor-logging/log.hpp>

#include "libpldm/base.h"

namespace pldm
{

namespace responder
{

namespace oem_ibm
{

void registerHandlers()
{
    registerHandler(PLDM_OEM, PLDM_GET_FILE_TABLE, std::move(getFileTable));
    registerHandler(PLDM_OEM, PLDM_READ_FILE_INTO_MEMORY,
                    std::move(readFileIntoMemory));
    registerHandler(PLDM_OEM, PLDM_WRITE_FILE_FROM_MEMORY,
                    std::move(writeFileFromMemory));
    registerHandler(PLDM_OEM, PLDM_READ_FILE, std::move(readFile));
    registerHandler(PLDM_OEM, PLDM_WRITE_FILE, std::move(writeFile));
    registerHandler(PLDM_OEM, PLDM_WRITE_FILE_TYPE_FROM_MEMORY,
                    std::move(writeFileByTypeFromMemory));
}

} // namespace oem_ibm

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
    uint64_t hostAddr; //!< the DMA address on the host side, configured by
                       //!< PCI subsystem.
    uint32_t len;      //!< the size of the transfer in bytes, it should be a
                       //!< multiple of 16 bytes
    uint32_t upstream; //!< boolean indicating the direction of the DMA
                       //!< operation, true means a transfer from BMC to host.
};

constexpr auto xdmaDev = "/dev/aspeed-xdma";

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
        std::ifstream stream(path.string(), std::ios::in | std::ios::binary);
        stream.seekg(offset);

        // Writing to the VGA memory should be aligned at page boundary,
        // otherwise write data into a buffer aligned at page boundary and
        // then write to the VGA memory.
        std::vector<char> buffer{};
        buffer.resize(pageAlignedLength);
        stream.read(buffer.data(), length);
        memcpy(static_cast<char*>(vgaMemPtr.get()), buffer.data(),
               pageAlignedLength);

        if (static_cast<uint32_t>(stream.gcount()) != length)
        {
            log<level::ERR>("mismatch between number of characters to read and "
                            "the length read",
                            entry("LENGTH=%d", length),
                            entry("COUNT=%d", stream.gcount()));
            return -1;
        }
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
                        entry("RC=%d", rc), entry("UPSTREAM=%d", upstream),
                        entry("ADDRESS=%lld", address),
                        entry("LENGTH=%d", length));
        return rc;
    }

    if (!upstream)
    {
        std::ios_base::openmode mode = std::ios::out | std::ios::binary;
        if (fs::exists(path))
        {
            mode |= std::ios::in;
        }
        std::ofstream stream(path.string(), mode);

        stream.seekp(offset);
        stream.write(static_cast<const char*>(vgaMemPtr.get()), length);
    }

    return 0;
}

} // namespace dma

Response readFileIntoMemory(const pldm_msg* request, size_t payloadLength)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint64_t address = 0;

    Response response((sizeof(pldm_msg_hdr) + PLDM_RW_FILE_MEM_RESP_BYTES), 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_RW_FILE_MEM_REQ_BYTES)
    {
        encode_rw_file_memory_resp(request->hdr.instance_id,
                                   PLDM_READ_FILE_INTO_MEMORY,
                                   PLDM_ERROR_INVALID_LENGTH, 0, responsePtr);
        return response;
    }

    decode_rw_file_memory_req(request, payloadLength, &fileHandle, &offset,
                              &length, &address);

    using namespace pldm::filetable;
    auto& table = buildFileTable(FILE_TABLE_JSON);
    FileEntry value{};

    try
    {
        value = table.at(fileHandle);
    }
    catch (std::exception& e)
    {
        log<level::ERR>("File handle does not exist in the file table",
                        entry("HANDLE=%d", fileHandle));
        encode_rw_file_memory_resp(request->hdr.instance_id,
                                   PLDM_READ_FILE_INTO_MEMORY,
                                   PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        return response;
    }

    if (!fs::exists(value.fsPath))
    {
        log<level::ERR>("File does not exist", entry("HANDLE=%d", fileHandle));
        encode_rw_file_memory_resp(request->hdr.instance_id,
                                   PLDM_READ_FILE_INTO_MEMORY,
                                   PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        return response;
    }

    auto fileSize = fs::file_size(value.fsPath);
    if (offset >= fileSize)
    {
        log<level::ERR>("Offset exceeds file size", entry("OFFSET=%d", offset),
                        entry("FILE_SIZE=%d", fileSize));
        encode_rw_file_memory_resp(request->hdr.instance_id,
                                   PLDM_READ_FILE_INTO_MEMORY,
                                   PLDM_DATA_OUT_OF_RANGE, 0, responsePtr);
        return response;
    }

    if (offset + length > fileSize)
    {
        length = fileSize - offset;
    }

    if (length % dma::minSize)
    {
        log<level::ERR>("Read length is not a multiple of DMA minSize",
                        entry("LENGTH=%d", length));
        encode_rw_file_memory_resp(request->hdr.instance_id,
                                   PLDM_READ_FILE_INTO_MEMORY,
                                   PLDM_INVALID_READ_LENGTH, 0, responsePtr);
        return response;
    }

    using namespace dma;
    DMA intf;
    return transferAll<DMA>(&intf, PLDM_READ_FILE_INTO_MEMORY, value.fsPath,
                            offset, length, address, true,
                            request->hdr.instance_id);
}

Response writeFileFromMemory(const pldm_msg* request, size_t payloadLength)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint64_t address = 0;

    Response response(sizeof(pldm_msg_hdr) + PLDM_RW_FILE_MEM_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_RW_FILE_MEM_REQ_BYTES)
    {
        encode_rw_file_memory_resp(request->hdr.instance_id,
                                   PLDM_WRITE_FILE_FROM_MEMORY,
                                   PLDM_ERROR_INVALID_LENGTH, 0, responsePtr);
        return response;
    }

    decode_rw_file_memory_req(request, payloadLength, &fileHandle, &offset,
                              &length, &address);

    if (length % dma::minSize)
    {
        log<level::ERR>("Write length is not a multiple of DMA minSize",
                        entry("LENGTH=%d", length));
        encode_rw_file_memory_resp(request->hdr.instance_id,
                                   PLDM_WRITE_FILE_FROM_MEMORY,
                                   PLDM_INVALID_WRITE_LENGTH, 0, responsePtr);
        return response;
    }

    using namespace pldm::filetable;
    auto& table = buildFileTable(FILE_TABLE_JSON);
    FileEntry value{};

    try
    {
        value = table.at(fileHandle);
    }
    catch (std::exception& e)
    {
        log<level::ERR>("File handle does not exist in the file table",
                        entry("HANDLE=%d", fileHandle));
        encode_rw_file_memory_resp(request->hdr.instance_id,
                                   PLDM_WRITE_FILE_FROM_MEMORY,
                                   PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        return response;
    }

    if (!fs::exists(value.fsPath))
    {
        log<level::ERR>("File does not exist", entry("HANDLE=%d", fileHandle));
        encode_rw_file_memory_resp(request->hdr.instance_id,
                                   PLDM_WRITE_FILE_FROM_MEMORY,
                                   PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        return response;
    }

    auto fileSize = fs::file_size(value.fsPath);
    if (offset >= fileSize)
    {
        log<level::ERR>("Offset exceeds file size", entry("OFFSET=%d", offset),
                        entry("FILE_SIZE=%d", fileSize));
        encode_rw_file_memory_resp(request->hdr.instance_id,
                                   PLDM_WRITE_FILE_FROM_MEMORY,
                                   PLDM_DATA_OUT_OF_RANGE, 0, responsePtr);
        return response;
    }

    using namespace dma;
    DMA intf;
    return transferAll<DMA>(&intf, PLDM_WRITE_FILE_FROM_MEMORY, value.fsPath,
                            offset, length, address, false,
                            request->hdr.instance_id);
}

Response getFileTable(const pldm_msg* request, size_t payloadLength)
{
    uint32_t transferHandle = 0;
    uint8_t transferFlag = 0;
    uint8_t tableType = 0;

    Response response(sizeof(pldm_msg_hdr) +
                      PLDM_GET_FILE_TABLE_MIN_RESP_BYTES);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_GET_FILE_TABLE_REQ_BYTES)
    {
        encode_get_file_table_resp(request->hdr.instance_id,
                                   PLDM_ERROR_INVALID_LENGTH, 0, 0, nullptr, 0,
                                   responsePtr);
        return response;
    }

    auto rc = decode_get_file_table_req(request, payloadLength, &transferHandle,
                                        &transferFlag, &tableType);
    if (rc)
    {
        encode_get_file_table_resp(request->hdr.instance_id, rc, 0, 0, nullptr,
                                   0, responsePtr);
        return response;
    }

    if (tableType != PLDM_FILE_ATTRIBUTE_TABLE)
    {
        encode_get_file_table_resp(request->hdr.instance_id,
                                   PLDM_INVALID_FILE_TABLE_TYPE, 0, 0, nullptr,
                                   0, responsePtr);
        return response;
    }

    using namespace pldm::filetable;
    auto table = buildFileTable(FILE_TABLE_JSON);
    auto attrTable = table();
    response.resize(response.size() + attrTable.size());
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (attrTable.empty())
    {
        encode_get_file_table_resp(request->hdr.instance_id,
                                   PLDM_FILE_TABLE_UNAVAILABLE, 0, 0, nullptr,
                                   0, responsePtr);
        return response;
    }

    encode_get_file_table_resp(request->hdr.instance_id, PLDM_SUCCESS, 0,
                               PLDM_START_AND_END, attrTable.data(),
                               attrTable.size(), responsePtr);
    return response;
}

Response readFile(const pldm_msg* request, size_t payloadLength)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;

    Response response(sizeof(pldm_msg_hdr) + PLDM_READ_FILE_RESP_BYTES);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_READ_FILE_REQ_BYTES)
    {
        encode_read_file_resp(request->hdr.instance_id,
                              PLDM_ERROR_INVALID_LENGTH, length, responsePtr);
        return response;
    }

    auto rc = decode_read_file_req(request, payloadLength, &fileHandle, &offset,
                                   &length);

    if (rc)
    {
        encode_read_file_resp(request->hdr.instance_id, rc, 0, responsePtr);
        return response;
    }

    using namespace pldm::filetable;
    auto& table = buildFileTable(FILE_TABLE_JSON);
    FileEntry value{};

    try
    {
        value = table.at(fileHandle);
    }
    catch (std::exception& e)
    {
        log<level::ERR>("File handle does not exist in the file table",
                        entry("HANDLE=%d", fileHandle));
        encode_read_file_resp(request->hdr.instance_id,
                              PLDM_INVALID_FILE_HANDLE, length, responsePtr);
        return response;
    }

    if (!fs::exists(value.fsPath))
    {
        log<level::ERR>("File does not exist", entry("HANDLE=%d", fileHandle));
        encode_read_file_resp(request->hdr.instance_id,
                              PLDM_INVALID_FILE_HANDLE, length, responsePtr);
        return response;
    }

    auto fileSize = fs::file_size(value.fsPath);
    if (offset >= fileSize)
    {
        log<level::ERR>("Offset exceeds file size", entry("OFFSET=%d", offset),
                        entry("FILE_SIZE=%d", fileSize));
        encode_read_file_resp(request->hdr.instance_id, PLDM_DATA_OUT_OF_RANGE,
                              length, responsePtr);
        return response;
    }

    if (offset + length > fileSize)
    {
        length = fileSize - offset;
    }

    response.resize(response.size() + length);
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    auto fileDataPos = reinterpret_cast<char*>(responsePtr);
    fileDataPos += sizeof(pldm_msg_hdr) + sizeof(uint8_t) + sizeof(length);

    std::ifstream stream(value.fsPath, std::ios::in | std::ios::binary);
    stream.seekg(offset);
    stream.read(fileDataPos, length);

    encode_read_file_resp(request->hdr.instance_id, PLDM_SUCCESS, length,
                          responsePtr);

    return response;
}

Response writeFile(const pldm_msg* request, size_t payloadLength)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    size_t fileDataOffset = 0;

    Response response(sizeof(pldm_msg_hdr) + PLDM_WRITE_FILE_RESP_BYTES);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength < PLDM_WRITE_FILE_REQ_BYTES)
    {
        encode_write_file_resp(request->hdr.instance_id,
                               PLDM_ERROR_INVALID_LENGTH, 0, responsePtr);
        return response;
    }

    auto rc = decode_write_file_req(request, payloadLength, &fileHandle,
                                    &offset, &length, &fileDataOffset);

    if (rc)
    {
        encode_write_file_resp(request->hdr.instance_id, rc, 0, responsePtr);
        return response;
    }

    using namespace pldm::filetable;
    auto& table = buildFileTable(FILE_TABLE_JSON);
    FileEntry value{};

    try
    {
        value = table.at(fileHandle);
    }
    catch (std::exception& e)
    {
        log<level::ERR>("File handle does not exist in the file table",
                        entry("HANDLE=%d", fileHandle));
        encode_write_file_resp(request->hdr.instance_id,
                               PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        return response;
    }

    if (!fs::exists(value.fsPath))
    {
        log<level::ERR>("File does not exist", entry("HANDLE=%d", fileHandle));
        encode_write_file_resp(request->hdr.instance_id,
                               PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        return response;
    }

    auto fileSize = fs::file_size(value.fsPath);
    if (offset >= fileSize)
    {
        log<level::ERR>("Offset exceeds file size", entry("OFFSET=%d", offset),
                        entry("FILE_SIZE=%d", fileSize));
        encode_write_file_resp(request->hdr.instance_id, PLDM_DATA_OUT_OF_RANGE,
                               0, responsePtr);
        return response;
    }

    auto fileDataPos =
        reinterpret_cast<const char*>(request->payload) + fileDataOffset;

    std::ofstream stream(value.fsPath,
                         std::ios::in | std::ios::out | std::ios::binary);
    stream.seekp(offset);
    stream.write(fileDataPos, length);

    encode_write_file_resp(request->hdr.instance_id, PLDM_SUCCESS, length,
                           responsePtr);

    return response;
}

constexpr auto invalidFileHandle = 0xFFFFFFFF;

Response writeFileByTypeFromMemory(const pldm_msg* request,
                                   size_t payloadLength)
{
    using namespace pldm::responder::oem_file_type;

    Response response(sizeof(pldm_msg_hdr) + PLDM_RW_FILE_TYPE_MEM_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_RW_FILE_TYPE_MEM_REQ_BYTES)
    {
        encode_rw_file_type_memory_resp(
            request->hdr.instance_id, PLDM_WRITE_FILE_TYPE_FROM_MEMORY,
            PLDM_ERROR_INVALID_LENGTH, 0, responsePtr);
        return response;
    }
    uint16_t fileType{};
    uint32_t fileHandle{};
    uint32_t offset{};
    uint32_t length{};
    uint64_t address{};
    auto rc =
        decode_rw_file_type_memory_req(request, payloadLength, &fileType,
                                       &fileHandle, &offset, &length, &address);
    if (rc != PLDM_SUCCESS)
    {
        encode_rw_file_type_memory_resp(
            request->hdr.instance_id, PLDM_WRITE_FILE_TYPE_FROM_MEMORY,
            PLDM_ERROR_INVALID_LENGTH, 0, responsePtr);
        return response;
    }
    if (length % dma::minSize)
    {
        log<level::ERR>("Write length is not a multiple of DMA minSize",
                        entry("LENGTH=%d", length));
        encode_rw_file_type_memory_resp(
            request->hdr.instance_id, PLDM_WRITE_FILE_TYPE_FROM_MEMORY,
            PLDM_INVALID_WRITE_LENGTH, 0, responsePtr);
        return response;
    }

    fs::path path{};
    if (fileHandle != invalidFileHandle)
    {
        using namespace pldm::filetable;
        auto& table = buildFileTable(FILE_TABLE_JSON);
        FileEntry value{};
        try
        {
            value = table.at(fileHandle);
        }
        catch (std::exception& e)
        {
            log<level::ERR>("File handle does not exist in the file table",
                            entry("HANDLE=%d", fileHandle));
            encode_rw_file_type_memory_resp(
                request->hdr.instance_id, PLDM_WRITE_FILE_TYPE_FROM_MEMORY,
                PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
            return response;
        }

        if (!fs::exists(value.fsPath))
        {
            log<level::ERR>("File does not exist",
                            entry("HANDLE=%d", fileHandle));
            encode_rw_file_type_memory_resp(
                request->hdr.instance_id, PLDM_WRITE_FILE_TYPE_FROM_MEMORY,
                PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
            return response;
        }
        auto fileSize = fs::file_size(value.fsPath);

        if (offset >= fileSize)
        {
            log<level::ERR>("Offset exceeds file size",
                            entry("OFFSET=%d", offset),
                            entry("FILE_SIZE=%d", fileSize));
            encode_rw_file_type_memory_resp(
                request->hdr.instance_id, PLDM_WRITE_FILE_TYPE_FROM_MEMORY,
                PLDM_DATA_OUT_OF_RANGE, 0, responsePtr);
            return response;
        }
        path = value.fsPath;
    }

    std::unique_ptr<FileHandler> handler;
    switch (fileType)
    {
        case PLDM_FILE_ERROR_LOG:
            handler = std::make_unique<PelHandler>();
            break;

        default:
        {
            encode_rw_file_type_memory_resp(request->hdr.instance_id,
                                            PLDM_INVALID_FILE_TYPE, rc, 0,
                                            responsePtr);
            return response;
        }
    }

    handler->setHandler(fileHandle, offset, length, address, path);
    using namespace dma;
    DMA intf;
    rc = handler->handle(&intf, PLDM_WRITE_FILE_TYPE_FROM_MEMORY, false);

    if (rc != PLDM_SUCCESS)
    {
        encode_rw_file_type_memory_resp(request->hdr.instance_id,
                                        PLDM_WRITE_FILE_TYPE_FROM_MEMORY, rc, 0,
                                        responsePtr);
        return response;
    }
    encode_rw_file_type_memory_resp(request->hdr.instance_id,
                                    PLDM_WRITE_FILE_TYPE_FROM_MEMORY,
                                    PLDM_SUCCESS, length, responsePtr);
    return response;
}

} // namespace responder
} // namespace pldm
