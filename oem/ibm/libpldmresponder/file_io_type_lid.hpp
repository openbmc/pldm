#pragma once

#include "config.h"

#include "file_io_by_type.hpp"

#include <filesystem>
#include <sstream>
#include <string>

namespace pldm
{
namespace responder
{

using namespace pldm::responder::dma;
namespace fs = std::filesystem;

/** @class LidHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to read/write LIDs.
 */
class LidHandler : public FileHandler
{
  public:
    /** @brief LidHandler constructor
     */
    LidHandler(uint32_t fileHandle, bool permSide) : FileHandler(fileHandle)
    {
        std::string dir = permSide ? LID_ALTERNATE_DIR : LID_RUNNING_DIR;
        std::stringstream stream;
        stream << std::hex << fileHandle;
        auto lidName = stream.str() + ".lid";
        std::string patchDir =
            permSide ? LID_ALTERNATE_PATCH_DIR : LID_RUNNING_PATCH_DIR;
        auto patch = fs::path(patchDir) / lidName;
        if (fs::is_regular_file(patch))
        {
            lidPath = patch;
        }
        else
        {
            lidPath = std::move(dir) + '/' + lidName;
        }
    }

    virtual int writeFromMemory(uint32_t /*offset*/, uint32_t /*length*/,
                                uint64_t /*address*/)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    virtual int readIntoMemory(uint32_t offset, uint32_t& length,
                               uint64_t address)
    {
        return transferFileData(lidPath, true, offset, length, address);
    }

    virtual int write(const char* /*buffer*/, uint32_t /*offset*/,
                      uint32_t& /*length*/)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    virtual int read(uint32_t offset, uint32_t& length, Response& response)
    {
        return readFile(lidPath, offset, length, response);
    }

    virtual int fileAck(uint8_t /*fileStatus*/)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    virtual int newFileAvailable(uint64_t /*length*/)

    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    /** @brief LidHandler destructor
     */
    ~LidHandler()
    {}

  protected:
    std::string lidPath;

  private:
    /** @brief Method to assemble code update images from LID files.
     *         It calls additional methods to assemble the different images such
     *         as host firmware and BMC.
     *  @param[in] lidPath - Path to the LID file to add to the image
     *  @return PLDM status code
     */
    int assembleImage(const std::string& lidPath);

    /** @brief Method to assemble a host firmware image from LID files.
     *  @param[in] lidPath - Path to the host fw LID file
     *  @return PLDM status code
     */
    int assembleHostFWImage(const std::string& lidPath);

    /** @brief Method to add a file to a squashfs image. It creates the squashfs
     *         image if it doesn't exist, otherwise it adds the file to existing
     *         image.
     *  @param[in] imagePath - Path to the squashfs image file to be created
     *  @param[in] filePath - Path to the file to be added to the squashfs image
     *  @return 0 if successful
     */
    int createSquashFSImage(const std::string& imagePath,
                            const std::string& filePath);
};

} // namespace responder
} // namespace pldm
