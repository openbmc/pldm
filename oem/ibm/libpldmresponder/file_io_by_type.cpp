#include "config.h"

#include "file_io_by_type.hpp"

#include "libpldmresponder/utils.hpp"

#include <stdint.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include <exception>
#include <filesystem>
#include <iostream>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/server.hpp>
#include <vector>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include "libpldm/base.h"
#include "oem/ibm/libpldm/file_io.h"

namespace pldm
{

namespace responder
{

using namespace phosphor::logging;

namespace oem_file_type
{

int PelHandler::handle(DMA* xdmaInterface, bool upstream)
{
    fs::create_directories(PEL_TEMP_DIR);

    auto timeMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();
    std::string fileName(PEL_TEMP_DIR);
    fileName += "/pel." + std::to_string(timeMs);

    fs::path path(fileName);

    auto origLength = this->getLength();
    auto offset = this->getOffset();
    auto address = this->getAddress();

    while (origLength > dma::maxSize)
    {
        auto rc = xdmaInterface->transferDataHost(path, offset, dma::maxSize,
                                                  address, upstream);
        if (rc > 0)
        {
            return PLDM_ERROR;
        }
        offset += dma::maxSize;
        origLength -= dma::maxSize;
        address += dma::maxSize;
    }
    auto rc = xdmaInterface->transferDataHost(path, offset, this->getLength(),
                                              address, upstream);
    if (rc < 0)
    {
        return PLDM_ERROR;
    }

    static constexpr const char* logObjPath = "/xyz/openbmc_project/logging";
    static constexpr const char* logInterface =
        "xyz.openbmc_project.Logging.Create";

    static sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

    static std::string service;
    std::map<std::string, std::string> addlData;

    if (service.empty())
    {
        try
        {
            service = getService(bus, logObjPath, logInterface);
        }
        catch (const sdbusplus::exception::SdBusError& e)
        {
            service.clear();
        }
    }

    using namespace sdbusplus::xyz::openbmc_project::Logging::server;
    addlData["RAWPEL"] = fileName;
    auto severity =
        sdbusplus::xyz::openbmc_project::Logging::server::convertForMessage(
            sdbusplus::xyz::openbmc_project::Logging::server::Entry::Level::
                Error);
    auto method = bus.new_method_call(service.c_str(), logObjPath, logInterface,
                                      "Create");
    method.append("pel from phyp", severity, addlData);
    bus.call_noreply(method);

    fs::remove(fileName);

    return PLDM_SUCCESS;
}

int LidHandler::handle(DMA* xdmaInterface, bool upstream)
{
    std::stringstream stream;
    stream << std::hex << this->getFileHandle();
    std::string lidName(stream.str());
    lidName += ".lid";
    char sep = '/';
    std::string lidPath(LID_TEMP_DIR);
    lidPath += sep + lidName;
    fs::path path(lidPath);

    if (!fs::exists(path))
    {
        log<level::ERR>("File does not exist", entry("PATH=%s", path.c_str()));
        return PLDM_INVALID_FILE_HANDLE;
    }
    auto fileSize = fs::file_size(path);
    auto offset = this->getOffset();
    auto address = this->getAddress();
    auto length = this->getLength();

    if (offset >= fileSize)
    {
        log<level::ERR>("Offset exceeds file size", entry("OFFSET=%d", offset),
                        entry("FILE_SIZE=%d", fileSize));
        return PLDM_DATA_OUT_OF_RANGE;
    }

    if (offset + length > fileSize)
    {
        length = fileSize - offset;
    }

    if (length % dma::minSize)
    {
        log<level::ERR>("Read length is not a multiple of DMA minSize",
                        entry("LENGTH=%d", length));
        return PLDM_INVALID_READ_LENGTH;
    }
    this->setLength(length);

    while (length > dma::maxSize)
    {
        auto rc = xdmaInterface->transferDataHost(path, offset, dma::maxSize,
                                                  address, upstream);
        if (rc < 0)
        {
            return PLDM_ERROR;
        }
        offset += dma::maxSize;
        length -= dma::maxSize;
        address += dma::maxSize;
    }
    auto rc = xdmaInterface->transferDataHost(path, offset, length, address,
                                              upstream);
    if (rc < 0)
    {
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

} // namespace oem_file_type

} // namespace responder

} // namespace pldm
