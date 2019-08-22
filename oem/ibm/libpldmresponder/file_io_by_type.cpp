#include "config.h"

#include "file_io_by_type.hpp"

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
using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;

namespace responder
{

namespace oem_file_type
{

int FileHandler::transferFileData(const fs::path& path, bool upstream)
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

std::unique_ptr<FileHandler> getHandlerByType(uint16_t fileType,
                                              uint32_t fileHandle,
                                              uint32_t offset, uint32_t length,
                                              uint64_t address)
{
    std::unique_ptr<FileHandler> handler = nullptr;
    switch (fileType)
    {
        case PLDM_FILE_ERROR_LOG:
            handler = std::make_unique<PelHandler>(fileHandle, offset, length,
                                                   address);
            break;
        default:
        {
            elog<InternalFailure>();
            break;
        }
    }
    return handler;
}

} // namespace oem_file_type

} // namespace responder

} // namespace pldm
