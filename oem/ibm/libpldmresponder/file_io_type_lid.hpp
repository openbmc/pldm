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
     *  @param[in] filePath - Path to the file to use for the image creation.
     *  @return PLDM status code
     */
    int assembleImage(const std::string& filePath);

    /** @brief Directory where the image files are stored as they are built */
    std::string imageDirPath = fs::path(LID_STAGING_DIR) / "image";

    /** @brief Directory where the lid files without a header are stored */
    std::string lidDirPath = fs::path(LID_STAGING_DIR) / "lid";

    /** @brief The file name of the hostfw image */
    std::string hostfwImageName = "image-host-fw";

    /** @brief The path to the hostfw image */
    std::string hostfwImagePath = fs::path(imageDirPath) / hostfwImageName;

    /** @struct lidHeader
     *  @brief LID header structure
     */
    struct lidHeader
    {
        uint16_t magicNumber;
        uint16_t headerVersion;
        uint32_t lidNumber;
        uint32_t lidDate;
        uint16_t lidTime;
        uint16_t lidClass;
        uint32_t lidCrc;
        uint32_t lidSize;
        uint32_t headerSize;
    };
};

} // namespace responder
} // namespace pldm
