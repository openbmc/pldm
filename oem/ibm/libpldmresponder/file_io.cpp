#include "config.h"

#include "file_io.hpp"

#include "file_table.hpp"
#include "libpldmresponder/utils.hpp"
#include "registration.hpp"

#include <cstring>
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
}

} // namespace oem_ibm

namespace fs = std::filesystem;
using namespace phosphor::logging;

namespace dma
{

namespace internal
{

DMA& intf()
{
    constexpr bool nonBlock = true;
    static DMA dmaIntf(nonBlock);
    return dmaIntf;
}

} // namespace internal

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

    int rc = 0;
    int xdmaFd = dmaFd();
    if (xdmaFd < 0)
    {
        log<level::ERR>("Failed to open the XDMA device",
                        entry("RC=%d", error()));
        return error();
    }

    void* mem = dmaAddr();
    if (mem == MAP_FAILED)
    {
        log<level::ERR>("Failed to mmap the XDMA device",
                        entry("RC=%d", error()));
        return error();
    }

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
        memcpy(static_cast<char*>(mem), buffer.data(), pageAlignedLength);

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

    rc = write(xdmaFd, &xdmaOp, sizeof(xdmaOp));
    if (rc < 0)
    {
        rc = -errno;
        log<level::ERR>("Failed to execute the DMA operation",
                        entry("RC=%d", rc), entry("UPSTREAM=%d", upstream),
                        entry("ADDRESS=%lld", address),
                        entry("LENGTH=%d", length));
        return rc;
    }

    return 0;
}

} // namespace dma

Response readFileIntoMemory(const Interfaces& intfs, const Request& request,
                            size_t payloadLength)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint64_t address = 0;

    Response response((sizeof(pldm_msg_hdr) + PLDM_RW_FILE_MEM_RESP_BYTES), 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_RW_FILE_MEM_REQ_BYTES)
    {
        encode_rw_file_memory_resp(request.msg->hdr.instance_id,
                                   PLDM_READ_FILE_INTO_MEMORY,
                                   PLDM_ERROR_INVALID_LENGTH, 0, responsePtr);
        return response;
    }

    decode_rw_file_memory_req(request.msg, payloadLength, &fileHandle, &offset,
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
        encode_rw_file_memory_resp(request.msg->hdr.instance_id,
                                   PLDM_READ_FILE_INTO_MEMORY,
                                   PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        return response;
    }

    if (!fs::exists(value.fsPath))
    {
        log<level::ERR>("File does not exist", entry("HANDLE=%d", fileHandle));
        encode_rw_file_memory_resp(request.msg->hdr.instance_id,
                                   PLDM_READ_FILE_INTO_MEMORY,
                                   PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        return response;
    }

    auto fileSize = fs::file_size(value.fsPath);
    if (offset >= fileSize)
    {
        log<level::ERR>("Offset exceeds file size", entry("OFFSET=%d", offset),
                        entry("FILE_SIZE=%d", fileSize));
        encode_rw_file_memory_resp(request.msg->hdr.instance_id,
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
        encode_rw_file_memory_resp(request.msg->hdr.instance_id,
                                   PLDM_READ_FILE_INTO_MEMORY,
                                   PLDM_INVALID_READ_LENGTH, 0, responsePtr);
        return response;
    }

    using namespace dma;
    transferAllAsync<DMA>(intfs, &internal::intf(), PLDM_READ_FILE_INTO_MEMORY,
                          value.fsPath, offset, length, address, true,
                          request.msg->hdr.instance_id, request.destinationId);
    return {};
}

Response writeFileFromMemory(const Interfaces& intfs, const Request& request,
                             size_t payloadLength)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint64_t address = 0;

    Response response(sizeof(pldm_msg_hdr) + PLDM_RW_FILE_MEM_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_RW_FILE_MEM_REQ_BYTES)
    {
        encode_rw_file_memory_resp(request.msg->hdr.instance_id,
                                   PLDM_WRITE_FILE_FROM_MEMORY,
                                   PLDM_ERROR_INVALID_LENGTH, 0, responsePtr);
        return response;
    }

    decode_rw_file_memory_req(request.msg, payloadLength, &fileHandle, &offset,
                              &length, &address);

    if (length % dma::minSize)
    {
        log<level::ERR>("Write length is not a multiple of DMA minSize",
                        entry("LENGTH=%d", length));
        encode_rw_file_memory_resp(request.msg->hdr.instance_id,
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
        encode_rw_file_memory_resp(request.msg->hdr.instance_id,
                                   PLDM_WRITE_FILE_FROM_MEMORY,
                                   PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        return response;
    }

    if (!fs::exists(value.fsPath))
    {
        log<level::ERR>("File does not exist", entry("HANDLE=%d", fileHandle));
        encode_rw_file_memory_resp(request.msg->hdr.instance_id,
                                   PLDM_WRITE_FILE_FROM_MEMORY,
                                   PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        return response;
    }

    auto fileSize = fs::file_size(value.fsPath);
    if (offset >= fileSize)
    {
        log<level::ERR>("Offset exceeds file size", entry("OFFSET=%d", offset),
                        entry("FILE_SIZE=%d", fileSize));
        encode_rw_file_memory_resp(request.msg->hdr.instance_id,
                                   PLDM_WRITE_FILE_FROM_MEMORY,
                                   PLDM_DATA_OUT_OF_RANGE, 0, responsePtr);
        return response;
    }

    using namespace dma;
    transferAllAsync<DMA>(intfs, &internal::intf(), PLDM_WRITE_FILE_FROM_MEMORY,
                          value.fsPath, offset, length, address, false,
                          request.msg->hdr.instance_id, request.destinationId);
    return {};
}

Response getFileTable(const Interfaces& intfs, const Request& request,
                      size_t payloadLength)
{
    uint32_t transferHandle = 0;
    uint8_t transferFlag = 0;
    uint8_t tableType = 0;

    Response response(sizeof(pldm_msg_hdr) +
                      PLDM_GET_FILE_TABLE_MIN_RESP_BYTES);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_GET_FILE_TABLE_REQ_BYTES)
    {
        encode_get_file_table_resp(request.msg->hdr.instance_id,
                                   PLDM_ERROR_INVALID_LENGTH, 0, 0, nullptr, 0,
                                   responsePtr);
        return response;
    }

    auto rc = decode_get_file_table_req(
        request.msg, payloadLength, &transferHandle, &transferFlag, &tableType);
    if (rc)
    {
        encode_get_file_table_resp(request.msg->hdr.instance_id, rc, 0, 0,
                                   nullptr, 0, responsePtr);
        return response;
    }

    if (tableType != PLDM_FILE_ATTRIBUTE_TABLE)
    {
        encode_get_file_table_resp(request.msg->hdr.instance_id,
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
        encode_get_file_table_resp(request.msg->hdr.instance_id,
                                   PLDM_FILE_TABLE_UNAVAILABLE, 0, 0, nullptr,
                                   0, responsePtr);
        return response;
    }

    encode_get_file_table_resp(request.msg->hdr.instance_id, PLDM_SUCCESS, 0,
                               PLDM_START_AND_END, attrTable.data(),
                               attrTable.size(), responsePtr);
    return response;
}

Response readFile(const Interfaces& intfs, const Request& request,
                  size_t payloadLength)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;

    Response response(sizeof(pldm_msg_hdr) + PLDM_READ_FILE_RESP_BYTES);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_READ_FILE_REQ_BYTES)
    {
        encode_read_file_resp(request.msg->hdr.instance_id,
                              PLDM_ERROR_INVALID_LENGTH, length, responsePtr);
        return response;
    }

    auto rc = decode_read_file_req(request.msg, payloadLength, &fileHandle,
                                   &offset, &length);

    if (rc)
    {
        encode_read_file_resp(request.msg->hdr.instance_id, rc, 0, responsePtr);
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
        encode_read_file_resp(request.msg->hdr.instance_id,
                              PLDM_INVALID_FILE_HANDLE, length, responsePtr);
        return response;
    }

    if (!fs::exists(value.fsPath))
    {
        log<level::ERR>("File does not exist", entry("HANDLE=%d", fileHandle));
        encode_read_file_resp(request.msg->hdr.instance_id,
                              PLDM_INVALID_FILE_HANDLE, length, responsePtr);
        return response;
    }

    auto fileSize = fs::file_size(value.fsPath);
    if (offset >= fileSize)
    {
        log<level::ERR>("Offset exceeds file size", entry("OFFSET=%d", offset),
                        entry("FILE_SIZE=%d", fileSize));
        encode_read_file_resp(request.msg->hdr.instance_id,
                              PLDM_DATA_OUT_OF_RANGE, length, responsePtr);
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

    encode_read_file_resp(request.msg->hdr.instance_id, PLDM_SUCCESS, length,
                          responsePtr);

    return response;
}

Response writeFile(const Interfaces& intfs, const Request& request,
                   size_t payloadLength)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    size_t fileDataOffset = 0;

    Response response(sizeof(pldm_msg_hdr) + PLDM_WRITE_FILE_RESP_BYTES);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength < PLDM_WRITE_FILE_REQ_BYTES)
    {
        encode_write_file_resp(request.msg->hdr.instance_id,
                               PLDM_ERROR_INVALID_LENGTH, 0, responsePtr);
        return response;
    }

    auto rc = decode_write_file_req(request.msg, payloadLength, &fileHandle,
                                    &offset, &length, &fileDataOffset);

    if (rc)
    {
        encode_write_file_resp(request.msg->hdr.instance_id, rc, 0,
                               responsePtr);
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
        encode_write_file_resp(request.msg->hdr.instance_id,
                               PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        return response;
    }

    if (!fs::exists(value.fsPath))
    {
        log<level::ERR>("File does not exist", entry("HANDLE=%d", fileHandle));
        encode_write_file_resp(request.msg->hdr.instance_id,
                               PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        return response;
    }

    auto fileSize = fs::file_size(value.fsPath);
    if (offset >= fileSize)
    {
        log<level::ERR>("Offset exceeds file size", entry("OFFSET=%d", offset),
                        entry("FILE_SIZE=%d", fileSize));
        encode_write_file_resp(request.msg->hdr.instance_id,
                               PLDM_DATA_OUT_OF_RANGE, 0, responsePtr);
        return response;
    }

    auto fileDataPos =
        reinterpret_cast<const char*>(request.msg->payload) + fileDataOffset;

    std::ofstream stream(value.fsPath,
                         std::ios::in | std::ios::out | std::ios::binary);
    stream.seekp(offset);
    stream.write(fileDataPos, length);

    encode_write_file_resp(request.msg->hdr.instance_id, PLDM_SUCCESS, length,
                           responsePtr);

    return response;
}

} // namespace responder
} // namespace pldm
