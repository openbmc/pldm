#include "config.h"

#include "file_io_by_type.hpp"

#include "file_io_type_lid.hpp"
#include "file_io_type_pel.hpp"
#include "libpldmresponder/utils.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <stdint.h>
#include <unistd.h>

#include <exception>
#include <filesystem>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <vector>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include "libpldm/base.h"
#include "oem/ibm/libpldm/file_io.h"

namespace pldm
{
namespace responder
{

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;

int FileHandler::transferFileData(const fs::path& path, bool upstream,
                                  uint32_t offset, uint32_t length,
                                  uint64_t address)
{
    if (upstream)
    {
        if (!fs::exists(path))
        {
            log<level::ERR>("File does not exist",
                            entry("PATH=%s", path.c_str()));
            return PLDM_INVALID_FILE_HANDLE;
        }

        size_t fileSize = fs::file_size(path);
        if (offset >= fileSize)
        {
            log<level::ERR>("Offset exceeds file size",
                            entry("OFFSET=%d", offset),
                            entry("FILE_SIZE=%d", fileSize));
            return PLDM_DATA_OUT_OF_RANGE;
        }
        if (offset + length > fileSize)
        {
            length = fileSize - offset;
        }
    }

    dma::DMA xdmaInterface;

    while (length > dma::maxSize)
    {
        auto rc = xdmaInterface.transferDataHost(path, offset, dma::maxSize,
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
        xdmaInterface.transferDataHost(path, offset, length, address, upstream);
    return rc < 0 ? PLDM_ERROR : PLDM_SUCCESS;
}

std::unique_ptr<FileHandler> getHandlerByType(uint16_t fileType,
                                              uint32_t fileHandle)
{
    switch (fileType)
    {
        case PLDM_FILE_TYPE_PEL:
        {
            return std::make_unique<PelHandler>(fileHandle);
            break;
        }
        case PLDM_FILE_TYPE_LID_PERM:
        {
            return std::make_unique<LidHandler>(fileHandle, true);
            break;
        }
        case PLDM_FILE_TYPE_LID_TEMP:
        {
            return std::make_unique<LidHandler>(fileHandle, false);
            break;
        }
        default:
        {
            elog<InternalFailure>();
            break;
        }
    }
    return nullptr;
}

} // namespace responder
} // namespace pldm
