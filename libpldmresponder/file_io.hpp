#pragma once

#include <stdint.h>
#include <unistd.h>

#include <filesystem>

#include "libpldm/base.h"
#include "libpldm/file_io.h"

namespace pldm
{

namespace responder
{

namespace fileio
{

void registerHandlers();

namespace utils
{

/** @struct CustomFD
 *
 *  RAII wrapper for file descriptor.
 */
struct CustomFD
{
    CustomFD(const CustomFD&) = delete;
    CustomFD& operator=(const CustomFD&) = delete;
    CustomFD(CustomFD&&) = delete;
    CustomFD& operator=(CustomFD&&) = delete;

    CustomFD(int fd) : fd(fd)
    {
    }

    ~CustomFD()
    {
        if (fd >= 0)
        {
            close(fd);
        }
    }

    int operator()() const
    {
        return fd;
    }

  private:
    int fd = -1;
};

} // namespace utils

namespace dma
{

// The minimum data size of dma transfer in bytes
constexpr uint32_t minSize = 16;

// 16MB - 4096B (16773120 bytes) is the maximum data size of DMA transfer
constexpr size_t maxSize = (16 * 1024 * 1024) - 4096;

namespace fs = std::filesystem;

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
 * @param[out] response - response message location
 */

template <class DMAInterface>
void transferAll(DMAInterface* intf, uint8_t command, fs::path& path,
                 uint32_t offset, uint32_t length, uint64_t address,
                 bool upstream, pldm_msg* response)
{
    uint32_t origLength = length;

    while (length > dma::maxSize)
    {
        auto rc = intf->transferDataHost(path, offset, dma::maxSize, address,
                                         upstream);
        if (rc < 0)
        {
            encode_rw_file_memory_resp(0, command, PLDM_ERROR, 0, response);
            return;
        }

        offset += dma::maxSize;
        length -= dma::maxSize;
        address += dma::maxSize;
    }

    auto rc = intf->transferDataHost(path, offset, length, address, upstream);
    if (rc < 0)
    {
        encode_rw_file_memory_resp(0, command, PLDM_ERROR, 0, response);
        return;
    }

    encode_rw_file_memory_resp(0, command, PLDM_SUCCESS, origLength, response);
    return;
}

} // namespace dma

/** @brief Handler for readFileIntoMemory command
 *
 *  @param[in] request - request message payload
 *  @param[out] response - response message location
 */
void readFileIntoMemory(const pldm_msg_payload* request, pldm_msg* response);

/** @brief Handler for writeFileIntoMemory command
 *
 *  @param[in] request - request message payload
 *  @param[out] response - response message location
 */
void writeFileFromMemory(const pldm_msg_payload* request, pldm_msg* response);

} // namespace fileio
} // namespace responder
} // namespace pldm
