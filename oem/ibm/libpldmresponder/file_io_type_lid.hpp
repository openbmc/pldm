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
        if (oemPlatformHandler != nullptr)
        {
            pldm::responder::oem_ibm_platform::Handler* oemIbmPlatformHandler =
                dynamic_cast<pldm::responder::oem_ibm_platform::Handler*>(
                    oemPlatformHandler);
            if (oemIbmPlatformHandler->codeUpdate->isCodeUpdateInProgress())
            {
                std::string dir = LID_STAGING_DIR;
                std::stringstream stream;
                stream << std::hex << fileHandle;
                auto lidName = stream.str() + ".lid";
                lidPath = std::move(dir) + '/' + lidName;
            }
        }
        std::cout << "got writeFromMemory() for LID " << lidPath.c_str()
                  << " and offset " << offset << " and length " << length
                  << "\n";
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
        auto fd = open(lidPath.c_str(), flags);
        if (fd == -1)
        {
            std::cerr << "Could not open file for writing  " << lidPath.c_str()
                      << "\n";
            return PLDM_ERROR;
        }
        close(fd);

        auto rc = transferFileData(lidPath, false, offset, length, address);
        if (rc != PLDM_SUCCESS)
        {
            std::cout << "writeFileFromMemory failed with rc= " << rc << " \n";
            return rc;
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
        if (oemPlatformHandler != nullptr)
        {
            pldm::responder::oem_ibm_platform::Handler* oemIbmPlatformHandler =
                dynamic_cast<pldm::responder::oem_ibm_platform::Handler*>(
                    oemPlatformHandler);
            if (oemIbmPlatformHandler->codeUpdate->isCodeUpdateInProgress())
            {
                std::string dir = LID_STAGING_DIR;
                std::stringstream stream;
                stream << std::hex << fileHandle;
                auto lidName = stream.str() + ".lid";
                lidPath = std::move(dir) + '/' + lidName;
            }
        }
        std::cout << "got write() call for LID " << lidPath.c_str()
                  << " and offset " << offset << " and length " << length
                  << "\n";
        bool fileExists = fs::exists(lidPath);
        int flags{};
        if (fileExists)
        {
            flags = O_RDWR;
            size_t fileSize = fs::file_size(lidPath);
            if (offset > fileSize)
            {
                std::cerr << "Offset exceeds file size, OFFSET=" << offset
                          << " FILE_SIZE=" << fileSize << "\n";
                return PLDM_DATA_OUT_OF_RANGE;
            }
        }
        else
        {
            flags = O_WRONLY | O_CREAT | O_TRUNC | O_SYNC;
            if (offset > 0)
            {
                std::cerr << "Offset is non zero in a new file \n";
                return PLDM_DATA_OUT_OF_RANGE;
            }
        }
        auto fd = open(lidPath.c_str(), flags);
        if (fd == -1)
        {
            std::cerr << "could not open file " << lidPath.c_str() << "\n";
            return PLDM_ERROR;
        }
        auto rc = lseek(fd, offset, SEEK_SET);
        if (rc == -1)
        {
            std::cerr << "lseek failed, ERROR=" << errno
                      << ", OFFSET=" << offset << "\n";
            return PLDM_ERROR;
        }
        std::cout << "lseek returned " << rc << "\n";
        rc = ::write(fd, buffer, length);
        if (rc == -1)
        {
            std::cerr << "file write failed, ERROR=" << errno
                      << ", LENGTH=" << length << ", OFFSET=" << offset << "\n";
            return PLDM_ERROR;
        }
        close(fd);
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
    std::string sideToRead;
    bool isPatchDir;
};

} // namespace responder
} // namespace pldm
