#pragma once

#include "config.h"

#include "file_io.hpp"

#include <stdint.h>
#include <unistd.h>

#include <filesystem>
#include <vector>

#include "libpldm/base.h"
#include "oem/ibm/libpldm/file_io.h"

namespace pldm
{

using namespace pldm::responder::dma;
namespace responder
{

namespace oem_file_type
{

class FileHandler
{
  public:
    virtual int handle(DMA* xdmaInterface, uint8_t command, bool upstream) = 0;

    void setHandler(uint32_t newfileHandle, uint32_t newOffset,
                    uint32_t newLength,
                    uint64_t newAddress) // use constructor instead
    {
        fileHandle = newfileHandle;
        offset = newOffset;
        length = newLength;
        address = newAddress;
    }
    uint32_t getLength()
    {
        return length;
    }

    uint32_t getOffset()
    {
        return offset;
    }

    uint64_t getAddress()
    {
        return address;
    }

    virtual ~FileHandler()
    {
    }

  private:
    uint32_t fileHandle;
    uint32_t offset;
    uint32_t length;
    uint64_t address;
};

class PelHandler : public FileHandler
{
  public:
    int handle(DMA* xdmaInterface, uint8_t command, bool upstream);

    ~PelHandler()
    {
    }
};

int PelHandler::handle(DMA* xdmaInterface, uint8_t command, bool upstream)
{
    constexpr auto maxPelFilesInTemp = 50;
    // later this should take care of the shm or different types of write/read
    // sources

    using fs::directory_iterator;
    auto currNumOfPels =
        std::distance(directory_iterator(PEL_TEMP_DIR), directory_iterator{});
    if (currNumOfPels > maxPelFilesInTemp)
    {
        // add a log here
        return PLDM_ERROR;
    }

    auto timeMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();
    std::string fileName(PEL_TEMP_DIR);
    fileName += "/pel." + std::to_string(timeMs);

    fs::path path(fileName);

    auto origLength = this->getLength();
    auto offset = this->getOffset();
    auto address = this->getAddress();

    while (origLength > dma::maxSize)
    {
        auto rc = xdmaInterface->transferDataHost(path, offset, dma::maxSize,
                                                  address, upstream);

        if (rc > 0)
        {
            return PLDM_ERROR;
        }
        offset += dma::maxSize;
        origLength -= dma::maxSize;
        address += dma::maxSize;
    }
    auto rc = xdmaInterface->transferDataHost(path, offset, this->getLength(),
                                              address, upstream);
    if (rc < 0)
    {
        return PLDM_ERROR;
    }
    return PLDM_SUCCESS;
}

} // namespace oem_file_type

} // namespace responder

} // namespace pldm
