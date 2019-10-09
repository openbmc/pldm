#pragma once

#include "file_io.hpp"

namespace pldm
{

using namespace pldm::responder::dma;
namespace responder
{

namespace oem_file_type
{

/**
 *  @class FileHandler
 *
 *  Base class to handle read/write of all oem file types
 *  which may/may not need to be maintained in a filetable.
 */
class FileHandler
{
  public:
    /** @brief Factory method to process an oem file type. Individual file types
     *  need to override this method to do the file specific processing
     *
     *  @param[in] xdmaInterface - DMA interface
     *  @param[in] upstream - indicates direction of the transfer; true
     * indicates transfer to the host
     *  @return PLDM status code
     */
    virtual int handle(DMA* xdmaInterface, bool upstream) = 0;

    /** @brief Set all the mandatory parameters to process a file
     *
     *  @param[in] newfileHandle - file handle
     *  @param[in] newOffset - offset in the file
     *  @param[in] newLength - length of the data to transfer
     *  @param[in] newAddress - DMA address on the host
     *  @param[in] newPath - pathname of the file to transfer data from or to
     *  @return none
     */
    void setHandler(uint32_t newfileHandle, uint32_t newOffset,
                    uint32_t newLength, uint64_t newAddress, fs::path newPath)
    {
        fileHandle = newfileHandle;
        offset = newOffset;
        length = newLength;
        address = newAddress;
        path = newPath;
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

    uint32_t getFileHandle()
    {
        return fileHandle;
    }

    void setLength(uint32_t newLength)
    {
        length = newLength;
    }

    virtual ~FileHandler()
    {
    }

  private:
    uint32_t fileHandle;
    uint32_t offset;
    uint32_t length;
    uint64_t address;
    fs::path path;
};

/** @class PelHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to read/write pels .
 */
class PelHandler : public FileHandler
{
  public:
    int handle(DMA* xdmaInterface, bool upstream);

    ~PelHandler()
    {
    }
};

/** @class LidHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to read/write lid files.
 */
class LidHandler : public FileHandler
{
  public:
    int handle(DMA* xdmaInterface, bool upstream);

    ~LidHandler()
    {
    }
};

} // namespace oem_file_type

} // namespace responder

} // namespace pldm
