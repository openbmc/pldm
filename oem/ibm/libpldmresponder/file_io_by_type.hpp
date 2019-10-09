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
 */
class FileHandler
{
  public:
    /** @brief Method to write an oem file type from host memory. Individual
     *  file types need to override this method to do the file specific
     *  processing
     *
     *  @return PLDM status code
     */
    virtual int writeFromMemory();

    /** @brief Method to read an oem file type into host memory. Individual
     *  file types need to override this method to do the file specific
     *  processing
     *
     *  @return PLDM status code
     */
    virtual int readIntoMemory();

    /** @brief Method to do the file content transfer ove DMA between host and
     *  bmc. This method is made virtual to be overridden in test case. And need
     *  not be defined in other child classes
     *
     *  @param[in] path - file system path  where read/write will be done
     *  @param[in] upstream - direction of DMA transfer. "false" means a
     *                        transfer from host to BMC
     *
     *  @return PLDM status code
     */
    virtual int transferFileData(fs::path& path, bool upstream);

    /** @brief Constructor to create a FileHandler object
     */
    FileHandler(uint32_t newfileHandle, uint32_t newOffset, uint32_t newLength,
                uint64_t newAddress, fs::path newPath) :
        fileHandle(newfileHandle),
        offset(newOffset), length(newLength), address(newAddress), path(newPath)
    {
    }

    /** FileHandler destructor
     */
    virtual ~FileHandler()
    {
    }

    /** @brief returns the length
     */
    uint32_t getLength()
    {
        return length;
    }

  protected:
    uint32_t fileHandle; //!< file handle indicating name of file or invalid
    uint32_t offset;     //!< from where read/write will be done
    uint32_t length;     //!< read/write length mentioned by host
    uint64_t address;    //!< address for DMA operation
    fs::path path;
};

/** @brief Method to create individual file handler objects based on file type
 *
 *  @param[in] fileType - type of file
 *  @param[in] fileHandle - file handle
 *  @param[in] offset - offset to read/write
 *  @param[in] length - length to be read/write mentioned by Host
 *  @param[in] path - location of the file
 *
 */

std::unique_ptr<FileHandler>
    getHandlerByType(uint16_t fileType, uint32_t& fileHandle, uint32_t& offset,
                     uint32_t& length, uint64_t& address, fs::path& path);

} // namespace oem_file_type

} // namespace responder

} // namespace pldm
