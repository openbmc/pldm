#pragma once

#include "file_io.hpp"

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

} // namespace oem_file_type

} // namespace responder

} // namespace pldm
