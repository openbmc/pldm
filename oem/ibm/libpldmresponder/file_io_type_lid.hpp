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

using MarkerLIDremainingSize = uint64_t;

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
    LidHandler(uint32_t fileHandle, bool permSide, uint8_t lidType = 0) :
        FileHandler(fileHandle), lidType(lidType)
    {
        sideToRead = permSide ? Pside : Tside;
        isPatchDir = false;
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
            isPatchDir = true;
        }
        else
        {
            lidPath = std::move(dir) + '/' + lidName;
        }
    }

    /** @brief Method to construct the LID path based on current boot side
     *  @param[in] oemPlatformHandler - OEM platform handler
     *  @return bool - true if a new path is constructed
     */
    bool constructLIDPath(oem_platform::Handler* oemPlatformHandler)
    {
        if (oemPlatformHandler != nullptr)
        {
            pldm::responder::oem_ibm_platform::Handler* oemIbmPlatformHandler =
                dynamic_cast<pldm::responder::oem_ibm_platform::Handler*>(
                    oemPlatformHandler);
            std::string dir = LID_ALTERNATE_DIR;
            if(isPatchDir)
            {
                dir = LID_ALTERNATE_PATCH_DIR;
            }

            if (oemIbmPlatformHandler->codeUpdate->fetchCurrentBootSide() ==
                sideToRead)
            {
                if(isPatchDir)
                {
                    dir = LID_RUNNING_PATCH_DIR;
                }
                else
                {
                    dir = LID_RUNNING_DIR;
                }
            }
            else if (oemIbmPlatformHandler->codeUpdate
                         ->isCodeUpdateInProgress())
            {
                return false;
            }

            std::stringstream stream;
            stream << std::hex << fileHandle;
            auto lidName = stream.str() + ".lid";
            lidPath = std::move(dir) + '/' + lidName;
        }
        return true;
    }

    virtual int writeFromMemory(uint32_t offset, uint32_t length,
                                uint64_t address,
                                oem_platform::Handler* oemPlatformHandler)
    {
        int rc = PLDM_SUCCESS;
        bool codeUpdateInProgress = false;
        if (oemPlatformHandler != nullptr)
        {
            pldm::responder::oem_ibm_platform::Handler* oemIbmPlatformHandler =
                dynamic_cast<pldm::responder::oem_ibm_platform::Handler*>(
                    oemPlatformHandler);
            codeUpdateInProgress =
                oemIbmPlatformHandler->codeUpdate->isCodeUpdateInProgress();
            if (codeUpdateInProgress || lidType == PLDM_FILE_TYPE_LID_MARKER)
            {
                std::string dir = LID_STAGING_DIR;
                std::stringstream stream;
                stream << std::hex << fileHandle;
                auto lidName = stream.str() + ".lid";
                lidPath = std::move(dir) + '/' + lidName;
            }
        }
        std::cout << "got writeFromMemory() for LID " << lidPath.c_str() 
                  << "\n";
        int flags = O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE;
        auto fd = open(lidPath.c_str(),flags);
        if (fd == -1)
        {
            std::cerr << "Could not open file for writing  " 
                      << lidPath.c_str() << "\n";
            return PLDM_ERROR;
        }
        //rc = transferFileData(lidPath, false, offset, length, address);
        rc = transferFileData(fd,false, offset, length, address);
        close(fd);
        if (rc != PLDM_SUCCESS)
        {
            return rc;
        }
        if (lidType == PLDM_FILE_TYPE_LID_MARKER)
        {
            markerLIDremainingSize -= length;
            if (markerLIDremainingSize == 0)
            {
                pldm::responder::oem_ibm_platform::Handler*
                    oemIbmPlatformHandler = dynamic_cast<
                        pldm::responder::oem_ibm_platform::Handler*>(
                        oemPlatformHandler);
                auto sensorId =
                    oemIbmPlatformHandler->codeUpdate->getMarkerLidSensor();
                using namespace pldm::responder::oem_ibm_platform;
                oemIbmPlatformHandler->sendStateSensorEvent(
                    sensorId, PLDM_STATE_SENSOR_STATE, 0, VALID, VALID);
                // rc = validate api;
            }
        }
        else if (codeUpdateInProgress)
        {
            std::cout << "calling assembleImage from writeFromMemory \n";
            rc = assembleImage(lidPath);
            std::cout << "assembleImage returned " << (uint32_t)rc << "\n";
        }
        return rc;
    }

    virtual int readIntoMemory(uint32_t offset, uint32_t& length,
                               uint64_t address,
                               oem_platform::Handler* oemPlatformHandler)
    {
        if (constructLIDPath(oemPlatformHandler))
        {
            return transferFileData(lidPath, true, offset, length, address);
        }
        return PLDM_ERROR;
    }

    virtual int write(const char* buffer, uint32_t offset, uint32_t& length,
                      oem_platform::Handler* oemPlatformHandler)
    {
        int rc = PLDM_SUCCESS;
        bool codeUpdateInProgress = false;
        if (oemPlatformHandler != nullptr)
        {
            pldm::responder::oem_ibm_platform::Handler* oemIbmPlatformHandler =
                dynamic_cast<pldm::responder::oem_ibm_platform::Handler*>(
                    oemPlatformHandler);
            codeUpdateInProgress =
                oemIbmPlatformHandler->codeUpdate->isCodeUpdateInProgress();
            if (codeUpdateInProgress || lidType == PLDM_FILE_TYPE_LID_MARKER)
            {
                std::string dir = LID_STAGING_DIR;
                std::stringstream stream;
                stream << std::hex << fileHandle;
                auto lidName = stream.str() + ".lid";
                lidPath = std::move(dir) + '/' + lidName;
            }
        }
        std::cout << "got write() call for LID " << lidPath.c_str() << "\n";
        std::ios_base::openmode flags =
            std::ios::in | std::ios::out | std::ios::binary;
        if (!fs::exists(lidPath))
        {
            flags = std::ios::out | std::ios::binary;
        }
        else
        {
            size_t fileSize = fs::file_size(lidPath);
        
            if (offset > fileSize)
            {
                std::cerr << "Offset exceeds file size, OFFSET=" << offset
                          << " FILE_SIZE=" << fileSize << "\n";
                return PLDM_DATA_OUT_OF_RANGE;
            }
        }

        std::ofstream stream(lidPath, flags);
        stream.seekp(offset);
        stream.write(buffer, length);
        stream.close();

        if (lidType == PLDM_FILE_TYPE_LID_MARKER)
        {
            markerLIDremainingSize -= length;
            if (markerLIDremainingSize == 0)
            {
                pldm::responder::oem_ibm_platform::Handler*
                    oemIbmPlatformHandler = dynamic_cast<
                        pldm::responder::oem_ibm_platform::Handler*>(
                        oemPlatformHandler);
                auto sensorId =
                    oemIbmPlatformHandler->codeUpdate->getMarkerLidSensor();
                using namespace pldm::responder::oem_ibm_platform;
                oemIbmPlatformHandler->sendStateSensorEvent(
                    sensorId, PLDM_STATE_SENSOR_STATE, 0, VALID, VALID);
                // validate api
            }
        }
        else if (codeUpdateInProgress)
        {
            std::cout << "calling assembleImage from write \n";
            rc = assembleImage(lidPath);
            std::cout << "assembleImage returned " << (uint32_t)rc << "\n";
        }

        return rc;
    }

    virtual int read(uint32_t offset, uint32_t& length, Response& response,
                     oem_platform::Handler* oemPlatformHandler)
    {
        if (constructLIDPath(oemPlatformHandler))
        {
            return readFile(lidPath, offset, length, response);
        }
        return PLDM_ERROR;
    }

    virtual int fileAck(uint8_t /*fileStatus*/)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    virtual int newFileAvailable(uint64_t length)

    {
        if (lidType == PLDM_FILE_TYPE_LID_MARKER)
        {
            markerLIDremainingSize = length;
            return PLDM_SUCCESS;
        }
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    /** @brief Method to assemble the code update tarball and trigger the
     *         phosphor software manager to create a version interface.
     *  @return PLDM status code
     */
    int assembleFinalImage();

    /** @brief LidHandler destructor
     */
    ~LidHandler()
    {}

  protected:
    std::string lidPath;
    std::string sideToRead;
    static inline MarkerLIDremainingSize markerLIDremainingSize;
    uint8_t lidType;
    bool isPatchDir;

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

    /** @brief Directory where the code update tarball files are stored */
    std::string updateDirPath = fs::path(LID_STAGING_DIR) / "update";

    /** @brief The file name of the code update tarball */
    std::string tarImageName = "image.tar";

    /** @brief The path to the code update tarball file */
    std::string tarImagePath = fs::path(imageDirPath) / tarImageName;

    /** @brief The file name of the hostfw image */
    std::string hostfwImageName = "image-host-fw";

    /** @brief The path to the hostfw image */
    std::string hostfwImagePath = fs::path(imageDirPath) / hostfwImageName;

    /** @brief The path to the tarball file expected by the phosphor software
     *         manager */
    std::string updateImagePath = fs::path("/tmp/images") / tarImageName;

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
