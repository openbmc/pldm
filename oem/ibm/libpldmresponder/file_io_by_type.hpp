#pragma once

#include "file_io.hpp"

namespace pldm
{

namespace responder
{

namespace fs = std::filesystem;

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
     *  @param[in] offset - offset to read/write
     *  @param[in] length - length to be read/write mentioned by Host
     *  @param[in] address - DMA address
     *  @return PLDM status code
     */
    virtual int writeFromMemory(uint32_t offset, uint32_t length,
                                uint64_t address) = 0;

    /** @brief Method to do the file content transfer ove DMA between host and
     *  bmc. This method is made virtual to be overridden in test case. And need
     *  not be defined in other child classes
     *
     *  @param[in] path - file system path  where read/write will be done
     *  @param[in] upstream - direction of DMA transfer. "false" means a
     *                        transfer from host to BMC
     *  @param[in] offset - offset to read/write
     *  @param[in] length - length to be read/write mentioned by Host
     *  @param[in] address - DMA address
     *
     *  @return PLDM status code
     */
    virtual int transferFileData(const fs::path& path, bool upstream,
                                 uint32_t offset, uint32_t length,
                                 uint64_t address);

    /** @brief Constructor to create a FileHandler object
     */
    FileHandler(uint32_t fileHandle) : fileHandle(fileHandle)
    {
    }

    /** FileHandler destructor
     */
    virtual ~FileHandler()
    {
    }

  protected:
    uint32_t fileHandle; //!< file handle indicating name of file or invalid
};

/** @brief Method to create individual file handler objects based on file type
 *
 *  @param[in] fileType - type of file
 *  @param[in] fileHandle - file handle
 */

std::unique_ptr<FileHandler> getHandlerByType(uint16_t fileType,
                                              uint32_t fileHandle);

} // namespace responder
} // namespace pldm
