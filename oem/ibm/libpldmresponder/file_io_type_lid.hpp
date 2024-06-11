#pragma once

#include "file_io_by_type.hpp"

#include <phosphor-logging/lg2.hpp>

#include <filesystem>
#include <sstream>
#include <string>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace responder
{

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
        std::string patchDir = permSide ? LID_ALTERNATE_PATCH_DIR
                                        : LID_RUNNING_PATCH_DIR;
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
            if (isPatchDir)
            {
                dir = LID_ALTERNATE_PATCH_DIR;
            }
            if (oemIbmPlatformHandler->codeUpdate->fetchCurrentBootSide() ==
                sideToRead)
            {
                if (isPatchDir)
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
        bool fileExists = fs::exists(lidPath);
        int flags{};
        if (fileExists)
        {
            flags = O_RDWR;
        }
        else
        {
            flags = O_WRONLY | O_CREAT | O_TRUNC | O_SYNC;
        }
        auto fd = open(lidPath.c_str(), flags, S_IRUSR);
        if (fd == -1)
        {
            error("Failed to open file '{LID_PATH}' for writing", "LID_PATH",
                  lidPath);
            return PLDM_ERROR;
        }
        close(fd);

        rc = transferFileData(lidPath, false, offset, length, address);
        if (rc != PLDM_SUCCESS)
        {
            error("Failed to write file from memory with response code '{RC}'",
                  "RC", rc);
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
                rc = PLDM_SUCCESS;
            }
        }
        else if (codeUpdateInProgress)
        {
            rc = processCodeUpdateLid(lidPath);
        }
        return rc;
    }

    virtual int readIntoMemory(uint32_t offset, uint32_t length,
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
        bool fileExists = fs::exists(lidPath);
        int flags{};
        if (fileExists)
        {
            flags = O_RDWR;
            size_t fileSize = fs::file_size(lidPath);
            if (offset > fileSize)
            {
                error(
                    "Offset '{OFFSET}' exceeds file size '{SIZE}' and file handle '{FILE_HANDLE}'",
                    "OFFSET", offset, "SIZE", fileSize, "FILE_HANDLE",
                    fileHandle);
                return PLDM_DATA_OUT_OF_RANGE;
            }
        }
        else
        {
            flags = O_WRONLY | O_CREAT | O_TRUNC | O_SYNC;
            if (offset > 0)
            {
                error("Offset '{OFFSET}' is non zero in a new file '{PATH}'",
                      "OFFSET", offset, "PATH", lidPath);
                return PLDM_DATA_OUT_OF_RANGE;
            }
        }
        auto fd = open(lidPath.c_str(), flags, S_IRUSR);
        if (fd == -1)
        {
            error("Failed to open file '{LID_PATH}'", "LID_PATH", lidPath);
            return PLDM_ERROR;
        }
        rc = lseek(fd, offset, SEEK_SET);
        if (rc == -1)
        {
            error(
                "Failed to lseek at offset '{OFFSET}', error number - {ERROR_NUM}",
                "ERROR_NUM", errno, "OFFSET", offset);
            return PLDM_ERROR;
        }
        rc = ::write(fd, buffer, length);
        if (rc == -1)
        {
            error(
                "Failed to do file write of length '{LENGTH}' at offset '{OFFSET}', error number - {ERROR_NUM}",
                "LENGTH", length, "OFFSET", offset, "ERROR_NUM", errno);
            return PLDM_ERROR;
        }
        else if (rc == static_cast<int>(length))
        {
            rc = PLDM_SUCCESS;
        }
        else if (rc < static_cast<int>(length))
        {
            rc = PLDM_ERROR;
        }
        close(fd);

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
                rc = PLDM_SUCCESS;
            }
        }
        else if (codeUpdateInProgress)
        {
            rc = processCodeUpdateLid(lidPath);
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

    virtual int newFileAvailableWithMetaData(uint64_t /*length*/,
                                             uint32_t /*metaDataValue1*/,
                                             uint32_t /*metaDataValue2*/,
                                             uint32_t /*metaDataValue3*/,
                                             uint32_t /*metaDataValue4*/)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    /** @brief LidHandler destructor
     */
    ~LidHandler() {}

  protected:
    std::string lidPath;
    std::string sideToRead;
    bool isPatchDir;
    static inline MarkerLIDremainingSize markerLIDremainingSize;
    uint8_t lidType;
};

} // namespace responder
} // namespace pldm
