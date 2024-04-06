#include "file_io.hpp"

#include "file_io_by_type.hpp"
#include "file_table.hpp"
#include "utils.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <fcntl.h>
#include <libpldm/base.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <cstring>
#include <fstream>
#include <memory>

PHOSPHOR_LOG2_USING;

namespace pldm
{
using namespace pldm::responder::utils;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;

namespace responder
{
namespace fs = std::filesystem;

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

int DMA::transferHostDataToSocket(int fd, uint32_t length, uint64_t address)
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

    int dmaFd = -1;
    int rc = 0;
    dmaFd = open(xdmaDev, O_RDWR);
    if (dmaFd < 0)
    {
        rc = -errno;
        error(
            "transferHostDataToSocket: Failed to open the XDMA device, RC={RC}",
            "RC", rc);
        return rc;
    }

    pldm::utils::CustomFD xdmaFd(dmaFd);

    void* vgaMem;
    vgaMem = mmap(nullptr, pageAlignedLength, PROT_READ, MAP_SHARED, xdmaFd(),
                  0);
    if (MAP_FAILED == vgaMem)
    {
        rc = -errno;
        error(
            "transferHostDataToSocket : Failed to mmap the XDMA device, RC={RC}",
            "RC", rc);
        return rc;
    }

    std::unique_ptr<void, decltype(mmapCleanup)> vgaMemPtr(vgaMem, mmapCleanup);

    AspeedXdmaOp xdmaOp;
    xdmaOp.upstream = 0;
    xdmaOp.hostAddr = address;
    xdmaOp.len = length;

    rc = write(xdmaFd(), &xdmaOp, sizeof(xdmaOp));
    if (rc < 0)
    {
        rc = -errno;
        error(
            "transferHostDataToSocket: Failed to execute the DMA operation, RC={RC} ADDRESS={ADDR} LENGTH={LEN}",
            "RC", rc, "ADDR", address, "LEN", length);
        return rc;
    }

    rc = writeToUnixSocket(fd, static_cast<const char*>(vgaMemPtr.get()),
                           length);
    if (rc < 0)
    {
        rc = -errno;
        close(fd);
        error(
            "transferHostDataToSocket: Closing socket as writeToUnixSocket faile with RC={RC}",
            "RC", rc);
        return rc;
    }
    return 0;
}

int DMA::transferDataHost(int fd, uint32_t offset, uint32_t length,
                          uint64_t address, bool upstream)
{
    static const size_t pageSize = getpagesize();
    uint32_t numPages = length / pageSize;
    uint32_t pageAlignedLength = numPages * pageSize;

    if (length > pageAlignedLength)
    {
        pageAlignedLength += pageSize;
    }

    int rc = 0;
    auto mmapCleanup = [pageAlignedLength, &rc](void* vgaMem) {
        if (rc != -EINTR)
        {
            munmap(vgaMem, pageAlignedLength);
        }
        else
        {
            error(
                "transferDataHost: Received interrupt during DMA transfer. Skipping Unmap.");
        }
    };

    int dmaFd = -1;
    dmaFd = open(xdmaDev, O_RDWR);
    if (dmaFd < 0)
    {
        rc = -errno;
        error("transferDataHost : Failed to open the XDMA device, RC={RC}",
              "RC", rc);
        return rc;
    }

    pldm::utils::CustomFD xdmaFd(dmaFd);

    void* vgaMem;
    vgaMem = mmap(nullptr, pageAlignedLength, upstream ? PROT_WRITE : PROT_READ,
                  MAP_SHARED, xdmaFd(), 0);
    if (MAP_FAILED == vgaMem)
    {
        rc = -errno;
        error("transferDataHost : Failed to mmap the XDMA device, RC={RC}",
              "RC", rc);
        return rc;
    }

    std::unique_ptr<void, decltype(mmapCleanup)> vgaMemPtr(vgaMem, mmapCleanup);

    if (upstream)
    {
        rc = lseek(fd, offset, SEEK_SET);
        if (rc == -1)
        {
            error(
                "transferDataHost upstream : lseek failed, ERROR={ERR}, UPSTREAM={UPSTREAM}, OFFSET={OFFSET}",
                "ERR", errno, "UPSTREAM", upstream, "OFFSET", offset);
            return rc;
        }

        // Writing to the VGA memory should be aligned at page boundary,
        // otherwise write data into a buffer aligned at page boundary and
        // then write to the VGA memory.
        std::vector<char> buffer{};
        buffer.resize(pageAlignedLength);
        rc = read(fd, buffer.data(), length);
        if (rc == -1)
        {
            error(
                "transferDataHost upstream : file read failed, ERROR={ERR}, UPSTREAM={UPSTREAM}, LENGTH={LEN}, OFFSET={OFFSET}",
                "ERR", errno, "UPSTREAM", upstream, "LEN", length, "OFFSET",
                offset);
            return rc;
        }
        if (rc != static_cast<int>(length))
        {
            error(
                "transferDataHost upstream : mismatch between number of characters to read and the length read, LENGTH={LEN} COUNT={RC}",
                "LEN", length, "RC", rc);
            return -1;
        }
        memcpy(static_cast<char*>(vgaMemPtr.get()), buffer.data(),
               pageAlignedLength);
    }

    AspeedXdmaOp xdmaOp;
    xdmaOp.upstream = upstream ? 1 : 0;
    xdmaOp.hostAddr = address;
    xdmaOp.len = length;

    rc = write(xdmaFd(), &xdmaOp, sizeof(xdmaOp));
    if (rc < 0)
    {
        rc = -errno;
        error(
            "transferDataHost : Failed to execute the DMA operation, RC={RC} UPSTREAM={UPSTREAM} ADDRESS={ADDR} LENGTH={LEN}",
            "RC", rc, "UPSTREAM", upstream, "ADDR", address, "LEN", length);
        return rc;
    }

    if (!upstream)
    {
        rc = lseek(fd, offset, SEEK_SET);
        if (rc == -1)
        {
            error(
                "transferDataHost downstream : lseek failed, ERROR={ERR}, UPSTREAM={UPSTREAM}, OFFSET={OFFSET}",
                "ERR", errno, "UPSTREAM", upstream, "OFFSET", offset);
            return rc;
        }
        rc = write(fd, static_cast<const char*>(vgaMemPtr.get()), length);
        if (rc == -1)
        {
            error(
                "transferDataHost downstream : file write failed, ERROR={ERR}, UPSTREAM={UPSTREAM}, LENGTH={LEN}, OFFSET={OFFSET}",
                "ERR", errno, "UPSTREAM", upstream, "LEN", length, "OFFSET",
                offset);
            return rc;
        }
    }

    return 0;
}

} // namespace dma

namespace oem_ibm
{
Response Handler::readFileIntoMemory(const pldm_msg* request,
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
        error(
            "Failed to read file into memory as payload length '{LENGTH}' not equal to '{REQ_LENGTH}'.",
            "LENGTH", payloadLength, "REQ_LENGTH", PLDM_RW_FILE_MEM_REQ_BYTES);
        int rc = encode_rw_file_memory_resp(
            request->hdr.instance_id, PLDM_READ_FILE_INTO_MEMORY,
            PLDM_ERROR_INVALID_LENGTH, 0, responsePtr);
        if (rc != PLDM_SUCCESS)
        {
            error(
                "Failed to encode read file into memory response, response code '{RC}'",
                "RC", rc);
        }
        return response;
    }

    int responseCode = decode_rw_file_memory_req(
        request, payloadLength, &fileHandle, &offset, &length, &address);
    if (responseCode != PLDM_SUCCESS)
    {
        error(
            "Failed to decode read file into memory request, response code '{RC}'",
            "RC", responseCode);
    }

    using namespace pldm::filetable;
    auto& table = buildFileTable(FILE_TABLE_JSON);
    FileEntry value{};

    try
    {
        value = table.at(fileHandle);
    }
    catch (const std::exception& e)
    {
        error("Handle ({HANDLE}) does not exist in the file table: {ERROR}",
              "HANDLE", fileHandle, "ERROR", e);
        int rc = encode_rw_file_memory_resp(
            request->hdr.instance_id, PLDM_READ_FILE_INTO_MEMORY,
            PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        if (rc != PLDM_SUCCESS)
        {
            error(
                "Failed to encode read file into memory response, response code '{RC}'",
                "RC", rc);
        }
        return response;
    }

    if (!fs::exists(value.fsPath))
    {
        error("File does not exist, HANDLE={FILE_HANDLE}", "FILE_HANDLE",
              fileHandle);
        int rc = encode_rw_file_memory_resp(
            request->hdr.instance_id, PLDM_READ_FILE_INTO_MEMORY,
            PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        if (rc != PLDM_SUCCESS)
        {
            error(
                "Failed to encode read file into memory response, response code '{RC}'",
                "RC", rc);
        }
        return response;
    }

    auto fileSize = fs::file_size(value.fsPath);
    if (fileSize == 0)
    {
        error(
            "Failed to read file into memory from file {PATH} with size '{SIZE}'",
            "PATH", value.fsPath, "SIZE", fileSize);
        int rc = encode_rw_file_memory_resp(
            request->hdr.instance_id, PLDM_READ_FILE_INTO_MEMORY,
            PLDM_DATA_OUT_OF_RANGE, 0, responsePtr);
        if (rc != PLDM_SUCCESS)
        {
            error(
                "Failed to encode read file into memory response, response code '{RC}'",
                "RC", rc);
        }
        return response;
    }
    if (offset >= fileSize)
    {
        error(
            "Offset exceeds file size, OFFSET={OFFSET} FILE_SIZE={FILE_SIZE} FILE_HANDLE{FILE_HANDLE}",
            "OFFSET", offset, "FILE_SIZE", fileSize, "FILE_HANDLE", fileHandle);
        int rc = encode_rw_file_memory_resp(
            request->hdr.instance_id, PLDM_READ_FILE_INTO_MEMORY,
            PLDM_DATA_OUT_OF_RANGE, 0, responsePtr);
        if (rc != PLDM_SUCCESS)
        {
            error(
                "Failed to encode read file into memory response, response code '{RC}'",
                "RC", rc);
        }
        return response;
    }

    if (offset + length > fileSize)
    {
        length = fileSize - offset;
    }

    if (length == 0 || length % dma::minSize)
    {
        error("Read length is not a multiple of DMA minSize, LENGTH={LEN}",
              "LEN", length);
        int rc = encode_rw_file_memory_resp(
            request->hdr.instance_id, PLDM_READ_FILE_INTO_MEMORY,
            PLDM_ERROR_INVALID_LENGTH, 0, responsePtr);
        if (rc != PLDM_SUCCESS)
        {
            error(
                "Failed to encode read file into memory response, response code '{RC}'",
                "RC", rc);
        }
        return response;
    }

    using namespace dma;
    DMA intf;
    return transferAll<DMA>(&intf, PLDM_READ_FILE_INTO_MEMORY, value.fsPath,
                            offset, length, address, true,
                            request->hdr.instance_id);
}

Response Handler::writeFileFromMemory(const pldm_msg* request,
                                      size_t payloadLength)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint64_t address = 0;
    info(" riya writeFileIntoMem");
    Response response(sizeof(pldm_msg_hdr) + PLDM_RW_FILE_MEM_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    if (payloadLength != PLDM_RW_FILE_MEM_REQ_BYTES)
    {
        error(
            "Failed to write file from memory as payload length '{LENGTH}' not equal to '{REQ_LENGTH}'.",
            "LENGTH", payloadLength, "REQ_LENGTH", PLDM_RW_FILE_MEM_REQ_BYTES);
        int rc = encode_rw_file_memory_resp(
            request->hdr.instance_id, PLDM_WRITE_FILE_FROM_MEMORY,
            PLDM_ERROR_INVALID_LENGTH, 0, responsePtr);
        if (rc != PLDM_SUCCESS)
        {
            error(
                "Failed to encode write file from memory response, response code '{RC}'",
                "RC", rc);
        }
        return response;
    }

    int responseCode = decode_rw_file_memory_req(
        request, payloadLength, &fileHandle, &offset, &length, &address);
    if (responseCode != PLDM_SUCCESS)
    {
        error(
            "Failed to decode write file from memory request, response code '{RC}'",
            "RC", responseCode);
    }

    if (length == 0 || length % dma::minSize)
    {
        error("Write length is not a multiple of DMA minSize, LENGTH={LEN}",
              "LEN", length);
        int rc = encode_rw_file_memory_resp(
            request->hdr.instance_id, PLDM_WRITE_FILE_FROM_MEMORY,
            PLDM_ERROR_INVALID_LENGTH, 0, responsePtr);
        if (rc != PLDM_SUCCESS)
        {
            error(
                "Failed to encode write file from memory response, response code '{RC}'",
                "RC", rc);
        }
        return response;
    }

    using namespace pldm::filetable;
    auto& table = buildFileTable(FILE_TABLE_JSON);
    FileEntry value{};

    try
    {
        value = table.at(fileHandle);
    }
    catch (const std::exception& e)
    {
        error(
            "File Handle '{HANDLE}' does not exist in the file table, error - {ERROR}",
            "HANDLE", fileHandle, "ERROR", e);
        int rc = encode_rw_file_memory_resp(
            request->hdr.instance_id, PLDM_WRITE_FILE_FROM_MEMORY,
            PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        if (rc != PLDM_SUCCESS)
        {
            error(
                "Failed to encode write file from memory response, response code '{RC}'",
                "RC", rc);
        }
        return response;
    }

    if (!fs::exists(value.fsPath))
    {
        error("File '{PATH}' with handle '{FILE_HANDLE}' does not exist.",
              "PATH", value.fsPath, "FILE_HANDLE", fileHandle);
        int rc = encode_rw_file_memory_resp(
            request->hdr.instance_id, PLDM_WRITE_FILE_FROM_MEMORY,
            PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        if (rc != PLDM_SUCCESS)
        {
            error(
                "Failed to encode write file from memory response, response code '{RC}'",
                "RC", rc);
        }
        return response;
    }

    auto fileSize = fs::file_size(value.fsPath);
    if (fileSize == 0)
    {
        info("File size for write file from memory has file size '{SIZE}'",
             "SIZE", fileSize);
    }
    if (offset >= fileSize)
    {
        error(
            "Offset exceeds file size, OFFSET={OFFSET} FILE_SIZE={FILE_SIZE} FILE_HANDLE{FILE_HANDLE}",
            "OFFSET", offset, "FILE_SIZE", fileSize, "FILE_HANDLE", fileHandle);
        int rc = encode_rw_file_memory_resp(
            request->hdr.instance_id, PLDM_WRITE_FILE_FROM_MEMORY,
            PLDM_DATA_OUT_OF_RANGE, 0, responsePtr);
        if (rc != PLDM_SUCCESS)
        {
            error(
                "Failed to encode write file from memory response, response code '{RC}'",
                "RC", rc);
        }
        return response;
    }

    using namespace dma;
    DMA intf;
    return transferAll<DMA>(&intf, PLDM_WRITE_FILE_FROM_MEMORY, value.fsPath,
                            offset, length, address, false,
                            request->hdr.instance_id);
}

Response Handler::getFileTable(const pldm_msg* request, size_t payloadLength)
{
    uint32_t transferHandle = 0;
    uint8_t transferFlag = 0;
    uint8_t tableType = 0;

    Response response(sizeof(pldm_msg_hdr) +
                      PLDM_GET_FILE_TABLE_MIN_RESP_BYTES);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_GET_FILE_TABLE_REQ_BYTES)
    {
        error(
            "Failed to get file table as payload length '{LENGTH}' not equal to required length '{REQ_LENGTH}'",
            "LENGTH", payloadLength, "REQ_LENGTH",
            PLDM_GET_FILE_TABLE_REQ_BYTES);
        int rc = encode_get_file_table_resp(request->hdr.instance_id,
                                            PLDM_ERROR_INVALID_LENGTH, 0, 0,
                                            nullptr, 0, responsePtr);
        if (rc != PLDM_SUCCESS)
        {
            error(
                "Failed to encode get file table response, response code '{RC}'",
                "RC", rc);
        }
        return response;
    }

    auto rc = decode_get_file_table_req(request, payloadLength, &transferHandle,
                                        &transferFlag, &tableType);
    if (rc)
    {
        error("Failed to decode get file table request, response code '{RC}'",
              "RC", rc);
        int responseCode = encode_get_file_table_resp(
            request->hdr.instance_id, rc, 0, 0, nullptr, 0, responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error(
                "Failed to encode get file table response, response code '{RC}'",
                "RC", responseCode);
        }
        return response;
    }

    if (tableType != PLDM_FILE_ATTRIBUTE_TABLE)
    {
        error(
            "Failed to match table type '{TYPE}' with expected table type '{REQ_TYPE}'",
            "TYPE", tableType, "REQ_TYPE", PLDM_FILE_ATTRIBUTE_TABLE);
        int responseCode = encode_get_file_table_resp(
            request->hdr.instance_id, PLDM_INVALID_FILE_TABLE_TYPE, 0, 0,
            nullptr, 0, responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error(
                "Failed to encode get file table response, response code '{RC}'",
                "RC", responseCode);
        }
        return response;
    }

    using namespace pldm::filetable;
    auto table = buildFileTable(FILE_TABLE_JSON);
    auto attrTable = table();
    response.resize(response.size() + attrTable.size());
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (attrTable.empty())
    {
        error("Attribute table is empty.");
        int responseCode = encode_get_file_table_resp(
            request->hdr.instance_id, PLDM_FILE_TABLE_UNAVAILABLE, 0, 0,
            nullptr, 0, responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error(
                "Failed to encode get file table response, response code '{RC}'",
                "RC", responseCode);
        }
        return response;
    }

    int responseCode = encode_get_file_table_resp(
        request->hdr.instance_id, PLDM_SUCCESS, 0, PLDM_START_AND_END,
        attrTable.data(), attrTable.size(), responsePtr);
    if (responseCode != PLDM_SUCCESS)
    {
        error("Failed to encode get file table response, response code '{RC}'",
              "RC", responseCode);
    }
    return response;
}

Response Handler::readFile(const pldm_msg* request, size_t payloadLength)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;

    Response response(sizeof(pldm_msg_hdr) + PLDM_READ_FILE_RESP_BYTES);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_READ_FILE_REQ_BYTES)
    {
        error(
            "Failed to read file as payload length '{LENGTH}' not equal to '{REQ_LENGTH}'.",
            "LENGTH", payloadLength, "REQ_LENGTH", PLDM_READ_FILE_REQ_BYTES);
        int rc = encode_read_file_resp(request->hdr.instance_id,
                                       PLDM_ERROR_INVALID_LENGTH, length,
                                       responsePtr);
        if (rc != PLDM_SUCCESS)
        {
            error("Failed to encode read file response, response code '{RC}'",
                  "RC", rc);
        }
        return response;
    }

    auto rc = decode_read_file_req(request, payloadLength, &fileHandle, &offset,
                                   &length);

    if (rc)
    {
        error("Failed to decode read file request, response code '{RC}'", "RC",
              rc);
        int responseCode = encode_read_file_resp(request->hdr.instance_id, rc,
                                                 0, responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error("Failed to encode read file response, response code '{RC}'",
                  "RC", responseCode);
        }
        return response;
    }

    using namespace pldm::filetable;
    auto& table = buildFileTable(FILE_TABLE_JSON);
    FileEntry value{};

    try
    {
        value = table.at(fileHandle);
    }
    catch (const std::exception& e)
    {
        error("Handle ({HANDLE}) does not exist in the file table: {ERROR}",
              "HANDLE", fileHandle, "ERROR", e);

        int responseCode = encode_read_file_resp(request->hdr.instance_id,
                                                 PLDM_INVALID_FILE_HANDLE,
                                                 length, responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error("Failed to encode read file response, response code '{RC}'",
                  "RC", responseCode);
        }
        return response;
    }

    if (!fs::exists(value.fsPath))
    {
        error("File '{PATH}' with handle '{FILE_HANDLE}' does not exist",
              "PATH", value.fsPath, "FILE_HANDLE", fileHandle);
        int responseCode = encode_read_file_resp(request->hdr.instance_id,
                                                 PLDM_INVALID_FILE_HANDLE,
                                                 length, responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error("Failed to encode read file response, response code '{RC}'",
                  "RC", responseCode);
        }
        return response;
    }

    auto fileSize = fs::file_size(value.fsPath);
    if (fileSize == 0)
    {
        error("Failed to read file {PATH} with size '{SIZE}'", "PATH",
              value.fsPath, "SIZE", fileSize);
        int rc = encode_rw_file_memory_resp(
            request->hdr.instance_id, PLDM_READ_FILE_INTO_MEMORY,
            PLDM_DATA_OUT_OF_RANGE, 0, responsePtr);
        if (rc != PLDM_SUCCESS)
        {
            error("Failed to encode read file response, response code '{RC}'",
                  "RC", rc);
        }
        return response;
    }

    if (offset >= fileSize)
    {
        error("Offset exceeds file size, OFFSET={OFFSET} FILE_SIZE={FILE_SIZE}",
              "OFFSET", offset, "FILE_SIZE", fileSize);
        int responseCode = encode_read_file_resp(request->hdr.instance_id,
                                                 PLDM_DATA_OUT_OF_RANGE, length,
                                                 responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error("Failed to encode read file response, response code '{RC}'",
                  "RC", responseCode);
        }
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

    int responseCode = encode_read_file_resp(request->hdr.instance_id,
                                             PLDM_SUCCESS, length, responsePtr);
    if (responseCode != PLDM_SUCCESS)
    {
        error("Failed to encode read file response, response code '{RC}'", "RC",
              responseCode);
    }

    return response;
}

Response Handler::writeFile(const pldm_msg* request, size_t payloadLength)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    size_t fileDataOffset = 0;

    Response response(sizeof(pldm_msg_hdr) + PLDM_WRITE_FILE_RESP_BYTES);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength < PLDM_WRITE_FILE_REQ_BYTES)
    {
        error(
            "Failed to write file as payload length '{LENGTH}' less than '{REQ_LENGTH}'.",
            "LENGTH", payloadLength, "REQ_LENGTH", PLDM_WRITE_FILE_REQ_BYTES);
        int responseCode = encode_write_file_resp(request->hdr.instance_id,
                                                  PLDM_ERROR_INVALID_LENGTH, 0,
                                                  responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error("Failed to encode write file response, response code '{RC}'",
                  "RC", responseCode);
        }
        return response;
    }

    auto rc = decode_write_file_req(request, payloadLength, &fileHandle,
                                    &offset, &length, &fileDataOffset);

    if (rc)
    {
        error("Failed to decode write file request, response code '{RC}'", "RC",
              rc);
        int responseCode = encode_write_file_resp(request->hdr.instance_id, rc,
                                                  0, responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error("Failed to encode write file response, response code '{RC}'",
                  "RC", responseCode);
        }
        return response;
    }

    using namespace pldm::filetable;
    auto& table = buildFileTable(FILE_TABLE_JSON);
    FileEntry value{};

    try
    {
        value = table.at(fileHandle);
    }
    catch (const std::exception& e)
    {
        error("Handle ({HANDLE}) does not exist in the file table: {ERROR}",
              "HANDLE", fileHandle, "ERROR", e);
        int responseCode = encode_write_file_resp(
            request->hdr.instance_id, PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error("Failed to encode write file response, response code '{RC}'",
                  "RC", responseCode);
        }
        return response;
    }

    if (!fs::exists(value.fsPath))
    {
        error("File does not exist, HANDLE={FILE_HANDLE}", "FILE_HANDLE",
              fileHandle);
        int responseCode = encode_write_file_resp(
            request->hdr.instance_id, PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error("Failed to encode write file response, response code '{RC}'",
                  "RC", responseCode);
        }
        return response;
    }

    auto fileSize = fs::file_size(value.fsPath);
    if (offset >= fileSize)
    {
        error("Offset exceeds file size, OFFSET={OFFSET} FILE_SIZE={FILE_SIZE}",
              "OFFSET", offset, "FILE_SIZE", fileSize);
        int responseCode = encode_write_file_resp(
            request->hdr.instance_id, PLDM_DATA_OUT_OF_RANGE, 0, responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error("Failed to encode write file response, response code '{RC}'",
                  "RC", responseCode);
        }
        return response;
    }

    auto fileDataPos = reinterpret_cast<const char*>(request->payload) +
                       fileDataOffset;

    std::ofstream stream(value.fsPath,
                         std::ios::in | std::ios::out | std::ios::binary);
    stream.seekp(offset);
    stream.write(fileDataPos, length);

    int responseCode = encode_write_file_resp(
        request->hdr.instance_id, PLDM_SUCCESS, length, responsePtr);
    if (responseCode != PLDM_SUCCESS)
    {
        error("Failed to encode write file response, response code '{RC}'",
              "RC", responseCode);
    }

    return response;
}

Response rwFileByTypeIntoMemory(uint8_t cmd, const pldm_msg* request,
                                size_t payloadLength,
                                oem_platform::Handler* oemPlatformHandler)
{
    info("riya rwbyType");
    Response response(
        sizeof(pldm_msg_hdr) + PLDM_RW_FILE_BY_TYPE_MEM_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_RW_FILE_BY_TYPE_MEM_REQ_BYTES)
    {
        encode_rw_file_by_type_memory_resp(request->hdr.instance_id, cmd,
                                           PLDM_ERROR_INVALID_LENGTH, 0,
                                           responsePtr);
        return response;
    }

    uint16_t fileType{};
    uint32_t fileHandle{};
    uint32_t offset{};
    uint32_t length{};
    uint64_t address{};
    auto rc = decode_rw_file_by_type_memory_req(request, payloadLength,
                                                &fileType, &fileHandle, &offset,
                                                &length, &address);
    if (rc != PLDM_SUCCESS)
    {
        error(
            "Failed to decode read/write file by type memory request, response code '{RC}'",
            "RC", rc);
        int responseCode = encode_rw_file_by_type_memory_resp(
            request->hdr.instance_id, cmd, rc, 0, responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error(
                "Failed to encode read/write file by type memory response, response code '{RC}'",
                "RC", responseCode);
        }
        return response;
    }

    if (length == 0 || length % dma::minSize)
    {
        error("Length is not a multiple of DMA minSize, LENGTH={LEN}", "LEN",
              length);
        int responseCode = encode_rw_file_by_type_memory_resp(
            request->hdr.instance_id, cmd, PLDM_ERROR_INVALID_LENGTH, 0,
            responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error(
                "Failed to encode read/write file by type memory response, response code '{RC}'",
                "RC", responseCode);
        }
        return response;
    }

    std::unique_ptr<FileHandler> handler{};
    try
    {
        handler = getHandlerByType(fileType, fileHandle);
    }
    catch (const InternalFailure& e)
    {
        error("Unknown file type '{FILE_TYPE}': {ERROR} ", "FILE_TYPE",
              fileType, "ERROR", e);
        int responseCode = encode_rw_file_by_type_memory_resp(
            request->hdr.instance_id, cmd, PLDM_INVALID_FILE_TYPE, 0,
            responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error(
                "Failed to encode read/write file by type memory response, response code '{RC}'",
                "RC", responseCode);
        }
        return response;
    }

    rc = cmd == PLDM_WRITE_FILE_BY_TYPE_FROM_MEMORY
             ? handler->writeFromMemory(offset, length, address,
                                        oemPlatformHandler)
             : handler->readIntoMemory(offset, length, address,
                                       oemPlatformHandler);
    int responseCode = encode_rw_file_by_type_memory_resp(
        request->hdr.instance_id, cmd, rc, length, responsePtr);
    if (responseCode != PLDM_SUCCESS)
    {
        error(
            "Failed to encode read/write file by type memory response, response code '{RC}'",
            "RC", responseCode);
    }
    return response;
}

Response Handler::writeFileByTypeFromMemory(const pldm_msg* request,
                                            size_t payloadLength)
{
    return rwFileByTypeIntoMemory(PLDM_WRITE_FILE_BY_TYPE_FROM_MEMORY, request,
                                  payloadLength, oemPlatformHandler);
}

Response Handler::readFileByTypeIntoMemory(const pldm_msg* request,
                                           size_t payloadLength)
{
    return rwFileByTypeIntoMemory(PLDM_READ_FILE_BY_TYPE_INTO_MEMORY, request,
                                  payloadLength, oemPlatformHandler);
}

Response Handler::writeFileByType(const pldm_msg* request, size_t payloadLength)
{
    Response response(sizeof(pldm_msg_hdr) + PLDM_RW_FILE_BY_TYPE_RESP_BYTES);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength < PLDM_RW_FILE_BY_TYPE_REQ_BYTES)
    {
        error(
            "Failed to write file by type as payload length '{LENGTH}' less than '{REQ_LENGTH}'.",
            "LENGTH", payloadLength, "REQ_LENGTH", PLDM_WRITE_FILE_REQ_BYTES);
        int responseCode = encode_rw_file_by_type_resp(
            request->hdr.instance_id, PLDM_WRITE_FILE_BY_TYPE,
            PLDM_ERROR_INVALID_LENGTH, 0, responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error(
                "Failed to encode write file by type response, response code '{RC}'",
                "RC", responseCode);
        }
        return response;
    }
    uint16_t fileType{};
    uint32_t fileHandle{};
    uint32_t offset{};
    uint32_t length{};

    auto rc = decode_rw_file_by_type_req(request, payloadLength, &fileType,
                                         &fileHandle, &offset, &length);
    if (rc != PLDM_SUCCESS)
    {
        error("Failed decoded write file by type request, response code '{RC}'",
              "RC", rc);
        int responseCode = encode_rw_file_by_type_resp(request->hdr.instance_id,
                                                       PLDM_WRITE_FILE_BY_TYPE,
                                                       rc, 0, responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error(
                "Failed to encode write file by type response, response code '{RC}'",
                "RC", responseCode);
        }
        return response;
    }

    std::unique_ptr<FileHandler> handler{};
    try
    {
        handler = getHandlerByType(fileType, fileHandle);
    }
    catch (const InternalFailure& e)
    {
        error("Unknown file type '{FILE_TYPE}': {ERROR}", "FILE_TYPE", fileType,
              "ERROR", e);
        int responseCode = encode_rw_file_by_type_resp(
            request->hdr.instance_id, PLDM_WRITE_FILE_BY_TYPE,
            PLDM_INVALID_FILE_TYPE, 0, responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error(
                "Failed to encode write file by type response, response code '{RC}'",
                "RC", responseCode);
        }
        return response;
    }

    rc = handler->write(reinterpret_cast<const char*>(
                            request->payload + PLDM_RW_FILE_BY_TYPE_REQ_BYTES),
                        offset, length, oemPlatformHandler);
    encode_rw_file_by_type_resp(request->hdr.instance_id,
                                PLDM_WRITE_FILE_BY_TYPE, rc, length,
                                responsePtr);
    return response;
}

Response Handler::readFileByType(const pldm_msg* request, size_t payloadLength)
{
    Response response(sizeof(pldm_msg_hdr) + PLDM_RW_FILE_BY_TYPE_RESP_BYTES);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_RW_FILE_BY_TYPE_REQ_BYTES)
    {
        error(
            "Failed to read file by type as payload length '{LENGTH}' less than '{REQ_LENGTH}'.",
            "LENGTH", payloadLength, "REQ_LENGTH",
            PLDM_RW_FILE_BY_TYPE_REQ_BYTES);
        int responseCode = encode_rw_file_by_type_resp(
            request->hdr.instance_id, PLDM_READ_FILE_BY_TYPE,
            PLDM_ERROR_INVALID_LENGTH, 0, responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error(
                "Failed to encode read file by type response, response code '{RC}'",
                "RC", responseCode);
        }
        return response;
    }
    uint16_t fileType{};
    uint32_t fileHandle{};
    uint32_t offset{};
    uint32_t length{};

    auto rc = decode_rw_file_by_type_req(request, payloadLength, &fileType,
                                         &fileHandle, &offset, &length);
    if (rc != PLDM_SUCCESS)
    {
        error(
            "Failed to decode read file by type request, response code '{RC}'",
            "RC", rc);
        int responseCode = encode_rw_file_by_type_resp(request->hdr.instance_id,
                                                       PLDM_READ_FILE_BY_TYPE,
                                                       rc, 0, responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error(
                "Failed to encode read file by type response, response code '{RC}'",
                "RC", responseCode);
        }
        return response;
    }

    std::unique_ptr<FileHandler> handler{};
    try
    {
        handler = getHandlerByType(fileType, fileHandle);
    }
    catch (const InternalFailure& e)
    {
        error("Unknown file type '{FILE_TYPE}': {ERROR}", "FILE_TYPE", fileType,
              "ERROR", e);
        int responseCode = encode_rw_file_by_type_resp(
            request->hdr.instance_id, PLDM_READ_FILE_BY_TYPE,
            PLDM_INVALID_FILE_TYPE, 0, responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error(
                "Failed to encode read file by type response, response code '{RC}'",
                "RC", responseCode);
        }
        return response;
    }

    rc = handler->read(offset, length, response, oemPlatformHandler);
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    int responseCode = encode_rw_file_by_type_resp(request->hdr.instance_id,
                                                   PLDM_READ_FILE_BY_TYPE, rc,
                                                   length, responsePtr);
    if (responseCode != PLDM_SUCCESS)
    {
        error(
            "Failed to encode read file by type response, response code '{RC}'",
            "RC", responseCode);
    }
    return response;
}

Response Handler::fileAck(const pldm_msg* request, size_t payloadLength)
{
    Response response(sizeof(pldm_msg_hdr) + PLDM_FILE_ACK_RESP_BYTES);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_FILE_ACK_REQ_BYTES)
    {
        error(
            "Failed to do file ack as payload length '{LENGTH}' is less than '{REQ_LENGTH}'.",
            "LENGTH", payloadLength, "REQ_LENGTH", PLDM_FILE_ACK_REQ_BYTES);
        int responseCode = encode_file_ack_resp(
            request->hdr.instance_id, PLDM_ERROR_INVALID_LENGTH, responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error("Failed to encode file ack response, response code '{RC}'",
                  "RC", responseCode);
        }
        return response;
    }
    uint16_t fileType{};
    uint32_t fileHandle{};
    uint8_t fileStatus{};

    auto rc = decode_file_ack_req(request, payloadLength, &fileType,
                                  &fileHandle, &fileStatus);
    if (rc != PLDM_SUCCESS)
    {
        int responseCode = encode_file_ack_resp(request->hdr.instance_id, rc,
                                                responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error("Failed to encode file ack response, response code '{RC}'",
                  "RC", responseCode);
        }
        return response;
    }

    std::unique_ptr<FileHandler> handler{};
    try
    {
        handler = getHandlerByType(fileType, fileHandle);
    }

    catch (const InternalFailure& e)
    {
        error("Unknown file type '{FILE_TYPE}': {ERROR}", "FILE_TYPE", fileType,
              "ERROR", e);
        int responseCode = encode_file_ack_resp(
            request->hdr.instance_id, PLDM_INVALID_FILE_TYPE, responsePtr);
        if (responseCode != PLDM_SUCCESS)
        {
            error("Failed to encode file ack response, response code '{RC}'",
                  "RC", responseCode);
        }
        return response;
    }

    rc = handler->fileAck(fileStatus);
    int responseCode = encode_file_ack_resp(request->hdr.instance_id, rc,
                                            responsePtr);
    if (responseCode != PLDM_SUCCESS)
    {
        error("Failed to encode file ack response, response code '{RC}'", "RC",
              responseCode);
    }
    return response;
}

Response Handler::getAlertStatus(const pldm_msg* request, size_t payloadLength)
{
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_ALERT_STATUS_RESP_BYTES);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    if (payloadLength != PLDM_GET_ALERT_STATUS_REQ_BYTES)
    {
        error(
            "Failed to get alert status as payload length '{LENGTH}' is less than '{REQ_LENGTH}'.",
            "LENGTH", payloadLength, "REQ_LENGTH",
            PLDM_GET_ALERT_STATUS_REQ_BYTES);
        return CmdHandler::ccOnlyResponse(request, PLDM_ERROR_INVALID_LENGTH);
    }

    uint8_t versionId{};

    auto rc = decode_get_alert_status_req(request, payloadLength, &versionId);
    if (rc != PLDM_SUCCESS)
    {
        error("Failed to decode get alert status request, response code '{RC}'",
              "RC", rc);
        return CmdHandler::ccOnlyResponse(request, rc);
    }

    if (versionId != 0)
    {
        error(
            "Failed to get alert status due to unsupported version ID '{VERSION}'",
            "VERSION", versionId);
        return CmdHandler::ccOnlyResponse(request,
                                          PLDM_HOST_UNSUPPORTED_FORMAT_VERSION);
    }

    constexpr uint32_t rackEntry = 0xFF000030;
    constexpr uint32_t priCecNode = 0x00008030;
    rc = encode_get_alert_status_resp(request->hdr.instance_id, PLDM_SUCCESS,
                                      rackEntry, priCecNode, responsePtr,
                                      PLDM_GET_ALERT_STATUS_RESP_BYTES);
    if (rc != PLDM_SUCCESS)
    {
        error(
            "Failed to encode get alert status response, response code '{RC}'",
            "RC", rc);
        return CmdHandler::ccOnlyResponse(request, rc);
    }

    return response;
}

Response Handler::newFileAvailable(const pldm_msg* request,
                                   size_t payloadLength)
{
    Response response(sizeof(pldm_msg_hdr) + PLDM_NEW_FILE_RESP_BYTES);

    if (payloadLength != PLDM_NEW_FILE_REQ_BYTES)
    {
        error(
            "Failed new file available as payload length '{LENGTH}' is less than '{REQ_LENGTH}'.",
            "LENGTH", payloadLength, "REQ_LENGTH", PLDM_NEW_FILE_REQ_BYTES);
        return CmdHandler::ccOnlyResponse(request, PLDM_ERROR_INVALID_LENGTH);
    }
    uint16_t fileType{};
    uint32_t fileHandle{};
    uint64_t length{};

    auto rc = decode_new_file_req(request, payloadLength, &fileType,
                                  &fileHandle, &length);

    if (rc != PLDM_SUCCESS)
    {
        error("Failed to decode new file request, response code '{RC}'", "RC",
              rc);
        return CmdHandler::ccOnlyResponse(request, rc);
    }

    std::unique_ptr<FileHandler> handler{};
    try
    {
        handler = getHandlerByType(fileType, fileHandle);
    }
    catch (const InternalFailure& e)
    {
        error("Unknown file type '{FILE_TYPE}': {ERROR}", "FILE_TYPE", fileType,
              "ERROR", e);
        return CmdHandler::ccOnlyResponse(request, PLDM_INVALID_FILE_TYPE);
    }

    rc = handler->newFileAvailable(length);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    int responseCode = encode_new_file_resp(request->hdr.instance_id, rc,
                                            responsePtr);
    if (responseCode != PLDM_SUCCESS)
    {
        error(
            "Failed to encode new file available response, response code '{RC}'",
            "RC", responseCode);
    }
    return response;
}

} // namespace oem_ibm
} // namespace responder
} // namespace pldm
