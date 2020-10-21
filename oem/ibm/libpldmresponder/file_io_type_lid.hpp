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
            if (oemIbmPlatformHandler->codeUpdate->fetchCurrentBootSide() ==
                sideToRead)
            {
                dir = LID_RUNNING_DIR;
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
        rc = transferFileData(lidPath, false, offset, length, address);
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
            rc = processCodeUpdateLid(lidPath);
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

    /** @brief LidHandler destructor
     */
    ~LidHandler()
    {}

  protected:
    std::string lidPath;
    std::string sideToRead;
    static inline MarkerLIDremainingSize markerLIDremainingSize;
    uint8_t lidType;
};

} // namespace responder
} // namespace pldm
