#include "file_io_by_type.hpp"

#include "common/utils.hpp"
#include "file_io_type_cert.hpp"
#include "file_io_type_dump.hpp"
#include "file_io_type_lid.hpp"
#include "file_io_type_pcie.hpp"
#include "file_io_type_pel.hpp"
#include "file_io_type_progress_src.hpp"
#include "file_io_type_vpd.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <libpldm/base.h>
#include <libpldm/oem/ibm/file_io.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace responder
{
using namespace sdbusplus::xyz::openbmc_project::Common::Error;

int FileHandler::transferFileData(int32_t fd, bool upstream, uint32_t offset,
                                  uint32_t& length, uint64_t address)
{
    dma::DMA xdmaInterface;
    while (length > dma::maxSize)
    {
        auto rc = xdmaInterface.transferDataHost(fd, offset, dma::maxSize,
                                                 address, upstream);
        if (rc < 0)
        {
            return PLDM_ERROR;
        }
        offset += dma::maxSize;
        length -= dma::maxSize;
        address += dma::maxSize;
    }
    auto rc =
        xdmaInterface.transferDataHost(fd, offset, length, address, upstream);
    return rc < 0 ? PLDM_ERROR : PLDM_SUCCESS;
}

int FileHandler::transferFileDataToSocket(int32_t fd, uint32_t& length,
                                          uint64_t address)
{
    dma::DMA xdmaInterface;
    while (length > dma::maxSize)
    {
        auto rc =
            xdmaInterface.transferHostDataToSocket(fd, dma::maxSize, address);
        if (rc < 0)
        {
            return PLDM_ERROR;
        }
        length -= dma::maxSize;
        address += dma::maxSize;
    }
    auto rc = xdmaInterface.transferHostDataToSocket(fd, length, address);
    return rc < 0 ? PLDM_ERROR : PLDM_SUCCESS;
}

int FileHandler::transferFileData(const fs::path& path, bool upstream,
                                  uint32_t offset, uint32_t& length,
                                  uint64_t address)
{
    bool fileExists = false;
    if (upstream)
    {
        fileExists = fs::exists(path);
        if (!fileExists)
        {
            error("File '{PATH}' does not exist.", "PATH", path);
            return PLDM_INVALID_FILE_HANDLE;
        }

        size_t fileSize = fs::file_size(path);
        if (offset >= fileSize)
        {
            error(
                "Offset '{OFFSET}' exceeds file size '{SIZE}' for file handle {FILE_HANDLE}",
                "OFFSET", offset, "SIZE", fileSize, "FILE_HANDLE", fileHandle);
            return PLDM_DATA_OUT_OF_RANGE;
        }
        if (offset + length > fileSize)
        {
            length = fileSize - offset;
        }
    }

    int flags{};
    if (upstream)
    {
        flags = O_RDONLY;
    }
    else if (fileExists)
    {
        flags = O_RDWR;
    }
    else
    {
        flags = O_WRONLY;
    }
    int file = open(path.string().c_str(), flags);
    if (file == -1)
    {
        error("File '{PATH}' does not exist.", "PATH", path);
        return PLDM_ERROR;
    }
    utils::CustomFD fd(file);

    return transferFileData(fd(), upstream, offset, length, address);
}

std::unique_ptr<FileHandler>
    getHandlerByType(uint16_t fileType, uint32_t fileHandle)
{
    switch (fileType)
    {
        case PLDM_FILE_TYPE_PEL:
        {
            return std::make_unique<PelHandler>(fileHandle);
        }
        case PLDM_FILE_TYPE_LID_PERM:
        {
            return std::make_unique<LidHandler>(fileHandle, true);
        }
        case PLDM_FILE_TYPE_LID_TEMP:
        {
            return std::make_unique<LidHandler>(fileHandle, false);
        }
        case PLDM_FILE_TYPE_LID_MARKER:
        {
            return std::make_unique<LidHandler>(fileHandle, false,
                                                PLDM_FILE_TYPE_LID_MARKER);
        }
        case PLDM_FILE_TYPE_DUMP:
        case PLDM_FILE_TYPE_RESOURCE_DUMP_PARMS:
        case PLDM_FILE_TYPE_RESOURCE_DUMP:
        {
            return std::make_unique<DumpHandler>(fileHandle, fileType);
        }
        case PLDM_FILE_TYPE_CERT_SIGNING_REQUEST:
        case PLDM_FILE_TYPE_SIGNED_CERT:
        case PLDM_FILE_TYPE_ROOT_CERT:
        {
            return std::make_unique<CertHandler>(fileHandle, fileType);
        }
        case PLDM_FILE_TYPE_PROGRESS_SRC:
        {
            return std::make_unique<ProgressCodeHandler>(fileHandle);
        }
        case PLDM_FILE_TYPE_LID_RUNNING:
        {
            return std::make_unique<LidHandler>(fileHandle, false,
                                                PLDM_FILE_TYPE_LID_RUNNING);
        }
        case PLDM_FILE_TYPE_PSPD_VPD_PDD_KEYWORD:
        {
            return std::make_unique<keywordHandler>(fileHandle, fileType);
        }
        case PLDM_FILE_TYPE_PCIE_TOPOLOGY:
        case PLDM_FILE_TYPE_CABLE_INFO:
        {
            return std::make_unique<PCIeInfoHandler>(fileHandle, fileType);
        }
        default:
        {
            throw InternalFailure();
            break;
        }
    }
    return nullptr;
}

int FileHandler::readFile(const std::string& filePath, uint32_t offset,
                          uint32_t& length, Response& response)
{
    if (!fs::exists(filePath))
    {
        error("File '{PATH}' and handle {FILE_HANDLE} does not exist", "PATH",
              filePath, "FILE_HANDLE", fileHandle);
        return PLDM_INVALID_FILE_HANDLE;
    }

    size_t fileSize = fs::file_size(filePath);
    if (offset >= fileSize)
    {
        error(
            "Offset '{OFFSET}' exceeds file size '{SIZE}' and file handle '{FILE_HANDLE}'",
            "OFFSET", offset, "SIZE", fileSize, "FILE_HANDLE", fileHandle);
        return PLDM_DATA_OUT_OF_RANGE;
    }

    if (offset + length > fileSize)
    {
        length = fileSize - offset;
    }

    size_t currSize = response.size();
    response.resize(currSize + length);
    auto filePos = reinterpret_cast<char*>(response.data());
    filePos += currSize;
    std::ifstream stream(filePath, std::ios::in | std::ios::binary);
    if (stream)
    {
        stream.seekg(offset);
        stream.read(filePos, length);
        return PLDM_SUCCESS;
    }
    error(
        "Unable to read file '{PATH}' at offset '{OFFSET}' for length '{LENGTH}'",
        "PATH", filePath, "OFFSET", offset, "LENGTH", length);
    return PLDM_ERROR;
}

} // namespace responder
} // namespace pldm
