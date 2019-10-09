#include "config.h"

#include "file_io_by_type.hpp"

#include "libpldmresponder/utils.hpp"
#include "lid_file_io.hpp"
#include "pel_file_io.hpp"
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
using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;

namespace responder
{

using namespace phosphor::logging;

namespace oem_file_type
{

int FileHandler::writeFromMemory()
{
    return PLDM_SUCCESS;
}

int FileHandler::readIntoMemory()
{
    return PLDM_SUCCESS;
}

int FileHandler::transferFileData(fs::path& path, bool upstream)
{
    using namespace dma;
    DMA xdmaInterface;
    auto origLength = this->length;
    auto offset = this->offset;
    auto address = this->address;

    while (origLength > dma::maxSize)
    {
        auto rc = xdmaInterface.transferDataHost(path, offset, dma::maxSize,
                                                 address, upstream);

        if (rc < 0)
        {
            return PLDM_ERROR;
        }
        offset += dma::maxSize;
        origLength -= dma::maxSize;
        address += dma::maxSize;
    }
    auto rc = xdmaInterface.transferDataHost(path, offset, origLength, address,
                                             upstream);

    if (rc < 0)
    {
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

std::unique_ptr<FileHandler>
    getHandlerByType(uint16_t fileType, uint32_t& fileHandle, uint32_t& offset,
                     uint32_t& length, uint64_t& address, fs::path& path)
{
    std::unique_ptr<FileHandler> handler = nullptr;
    switch (fileType)
    {
        case PLDM_FILE_ERROR_LOG:
            handler = std::make_unique<PelHandler>(fileHandle, offset, length,
                                                   address, path);
            break;
        case PLDM_FILE_LID:
        {
            auto lidpath = createLidPath(fileHandle);
            handler = std::make_unique<LidHandler>(fileHandle, offset, length,
                                                   address, lidpath);
        }
        break;
        default:
        {
            log<level::ERR>("Passed invalid file type to getHandlerByType ",
                            entry("FILE TYPE=%d", fileType));
            elog<InternalFailure>();
            break;
        }
    }
    return handler;
}

} // namespace oem_file_type

} // namespace responder

} // namespace pldm
