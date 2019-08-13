#pragma once

#include "pldmd_api.hpp"
#include "registration.hpp"

#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <sdeventplus/source/io.hpp>
#include <vector>

#include "libpldm/base.h"
#include "oem/ibm/libpldm/file_io.h"

namespace pldm
{

namespace responder
{

namespace oem_ibm
{
/** @brief Register handlers for command from the platform spec
 */
void registerHandlers();
} // namespace oem_ibm

namespace dma
{

// The minimum data size of dma transfer in bytes
constexpr uint32_t minSize = 16;

// 16MB - 4096B (16773120 bytes) is the maximum data size of DMA transfer
constexpr size_t maxSize = (16 * 1024 * 1024) - 4096;

namespace fs = std::filesystem;
using namespace sdeventplus::source;
using namespace daemon_api;

struct IOPart
{
    uint32_t length;
    uint32_t offset;
    uint64_t address;
};

/**
 * @class DMA
 *
 * Expose API to initiate transfer of data by DMA
 *
 * This class only exposes the public API transferDataHost to transfer data
 * between BMC and host using DMA. This allows for mocking the transferDataHost
 * for unit testing purposes.
 */
class DMA
{
  public:
    DMA(bool nonBlock = false)
    {
        int modes = O_RDWR;
        if (nonBlock)
        {
            modes |= O_NONBLOCK;
        }
        xdmaFd = open("/dev/xdma", modes);
        if (xdmaFd < 0)
        {
            rc = -errno;
        }
        else
        {
            addr = mmap(nullptr, maxSize, PROT_WRITE | PROT_READ, MAP_SHARED,
                        xdmaFd, 0);
            if (MAP_FAILED == addr)
            {
                rc = -errno;
            }
        }
    }

    ~DMA()
    {
        if (addr && addr != MAP_FAILED)
        {
            munmap(addr, maxSize);
        }
        if (xdmaFd != -1)
        {
            close(xdmaFd);
        }
    }

    int dmaFd() const
    {
        return xdmaFd;
    }

    void* dmaAddr() const
    {
        return addr;
    }

    int error() const
    {
        return rc;
    }

    /** @brief API to transfer data between BMC and host using DMA
     *
     * @param[in] path     - pathname of the file to transfer data from or to
     * @param[in] offset   - offset in the file
     * @param[in] length   - length of the data to transfer
     * @param[in] address  - DMA address on the host
     * @param[in] upstream - indicates direction of the transfer; true indicates
     *                       transfer to the host
     *
     * @return returns 0 on success, negative errno on failure
     */
    int transferDataHost(const fs::path& path, uint32_t offset, uint32_t length,
                         uint64_t address, bool upstream);

  private:
    int xdmaFd = -1;
    void* addr = nullptr;
    int rc = 0;
};

/** @brief Transfer the data between BMC and host using DMA.
 *
 *  There is a max size for each DMA operation, transferAll API abstracts this
 *  and the requested length is broken down into multiple DMA operations if the
 *  length exceed max size.
 *
 * @tparam[in] T - DMA interface type
 * @param[in] intf - interface passed to invoke DMA transfer
 * @param[in] command  - PLDM command
 * @param[in] path     - pathname of the file to transfer data from or to
 * @param[in] offset   - offset in the file
 * @param[in] length   - length of the data to transfer
 * @param[in] address  - DMA address on the host
 * @param[in] upstream - indicates direction of the transfer; true indicates
 *                       transfer to the host
 * @param[in] instanceId - Message's instance id
 * @return PLDM response message
 */

template <class DMAInterface>
Response transferAll(DMAInterface* intf, uint8_t command, fs::path& path,
                     uint32_t offset, uint32_t length, uint64_t address,
                     bool upstream, uint8_t instanceId)
{
    uint32_t origLength = length;
    Response response(sizeof(pldm_msg_hdr) + PLDM_RW_FILE_MEM_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    while (length > dma::maxSize)
    {
        auto rc = intf->transferDataHost(path, offset, dma::maxSize, address,
                                         upstream);
        if (rc < 0)
        {
            encode_rw_file_memory_resp(instanceId, command, PLDM_ERROR, 0,
                                       responsePtr);
            return response;
        }

        offset += dma::maxSize;
        length -= dma::maxSize;
        address += dma::maxSize;
    }

    auto rc = intf->transferDataHost(path, offset, length, address, upstream);
    if (rc < 0)
    {
        encode_rw_file_memory_resp(instanceId, command, PLDM_ERROR, 0,
                                   responsePtr);
        return response;
    }

    encode_rw_file_memory_resp(instanceId, command, PLDM_SUCCESS, origLength,
                               responsePtr);
    return response;
}

/** @brief Transfer the data between BMC and host using DMA asynchronously
 *
 *  There is a max size for each DMA operation, transferAllAsync API abstracts
 * this and the requested length is broken down into multiple DMA operations if
 * the length exceed max size.
 *
 * @tparam[in] T - DMA interface type
 * @param[in] intf - interface passed to invoke DMA transfer
 * @param[in] command  - PLDM command
 * @param[in] path     - pathname of the file to transfer data from or to
 * @param[in] offset   - offset in the file
 * @param[in] length   - length of the data to transfer
 * @param[in] address  - DMA address on the host
 * @param[in] upstream - indicates direction of the transfer; true indicates
 *                       transfer to the host
 * @param[in] instanceId - Message's instance id
 */

template <class DMAInterface>
void transferAllAsync(const Interfaces& intfs, DMAInterface* intf,
                      uint8_t command, fs::path& path, uint32_t offset,
                      uint32_t length, uint64_t address, bool upstream,
                      uint8_t instanceId, uint8_t destinationId)
{
    uint32_t origLength = length;
    Response response(sizeof(pldm_msg_hdr) + PLDM_RW_FILE_MEM_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    static IOPart part;
    part.length = length;
    part.offset = offset;
    part.address = address;
    auto transport = intfs.transport.get();
    auto eventLoop = intfs.eventLoop.get();

    auto callback = [=](IO& io, int fd, uint32_t revents) {
        Response response(sizeof(pldm_msg_hdr) + PLDM_RW_FILE_MEM_RESP_BYTES,
                          0);
        if (part.length > dma::maxSize)
        {
            part.length -= dma::maxSize;
            part.offset += dma::maxSize;
            part.address += dma::maxSize;

            auto rc = intf->transferDataHost(
                path, part.offset,
                part.length > dma::maxSize ? dma::maxSize : part.length,
                part.address, upstream);
            if (rc < 0)
            {
                encode_rw_file_memory_resp(instanceId, command, PLDM_ERROR, 0,
                                           responsePtr);
                transport->sendPLDMMsg(destinationId, response);
            }
        }
        else
        {
            if (!upstream)
            {
                std::ofstream stream(path.string(), std::ios::in |
                                                        std::ios::out |
                                                        std::ios::binary);

                stream.seekp(part.offset);
                stream.write(static_cast<const char*>(intf->dmaAddr()),
                             part.length);
            }
            encode_rw_file_memory_resp(instanceId, command, PLDM_SUCCESS,
                                       origLength, responsePtr);
            transport->sendPLDMMsg(destinationId, response);
        }
    };

    static IO io(*eventLoop, intf->dmaFd(), EPOLLIN | EPOLLOUT,
                 std::move(callback));

    auto rc = intf->transferDataHost(
        path, offset, length > dma::maxSize ? dma::maxSize : length, address,
        upstream);
    if (rc < 0)
    {
        encode_rw_file_memory_resp(instanceId, command, PLDM_ERROR, 0,
                                   responsePtr);
        transport->sendPLDMMsg(destinationId, response);
    }
}

} // namespace dma

/** @brief Handler for readFileIntoMemory command
 *
 *  @param[in] request - pointer to PLDM request payload
 *  @param[in] payloadLength - length of the message
 *
 *  @return PLDM response message
 */
Response readFileIntoMemory(const Interfaces& intfs, const Request& request,
                            size_t payloadLength);

/** @brief Handler for writeFileIntoMemory command
 *
 *  @param[in] request - pointer to PLDM request payload
 *  @param[in] payloadLength - length of the message
 *
 *  @return PLDM response message
 */
Response writeFileFromMemory(const Interfaces& intfs, const Request& request,
                             size_t payloadLength);

/** @brief Handler for GetFileTable command
 *
 *  @param[in] request - pointer to PLDM request payload
 *  @param[in] payloadLength - length of the message payload
 *
 *  @return PLDM response message
 */
Response getFileTable(const Interfaces& intfs, const Request& request,
                      size_t payloadLength);

/** @brief Handler for readFile command
 *
 *  @param[in] request - PLDM request msg
 *  @param[in] payloadLength - length of the message payload
 *
 *  @return PLDM response message
 */
Response readFile(const Interfaces& intfs, const Request& request,
                  size_t payloadLength);

/** @brief Handler for writeFile command
 *
 *  @param[in] request - PLDM request msg
 *  @param[in] payloadLength - length of the message payload
 *
 *  @return PLDM response message
 */
Response writeFile(const Interfaces& intfs, const Request& request,
                   size_t payloadLength);
} // namespace responder
} // namespace pldm
