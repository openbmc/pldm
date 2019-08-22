#include "config.h"

#include "file_io_by_type.hpp"

#include "libpldmresponder/utils.hpp"

#include <stdint.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include <exception>
#include <filesystem>
#include <sdbusplus/server.hpp>
#include <vector>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include "libpldm/base.h"
#include "oem/ibm/libpldm/file_io.h"

namespace pldm
{

namespace responder
{

namespace oem_file_type
{

int PelHandler::handle(DMA* xdmaInterface, uint8_t command, bool upstream)
{
    command = command;
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

} // namespace oem_file_type

} // namespace responder

} // namespace pldm
