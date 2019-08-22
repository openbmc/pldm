#include "config.h"

#include "pel_file_io.hpp"

#include "libpldmresponder/utils.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <stdint.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include <exception>
#include <filesystem>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/server.hpp>
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

int PelHandler::writeFromMemory()
{
    fs::create_directories(PEL_TEMP_DIR);

    auto timeMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();

    std::string fileName(PEL_TEMP_DIR);
    fileName += "/pel." + std::to_string(timeMs);

    fs::path path(fileName);
    pelFileName = fileName;

    auto rc = this->transferFileData(path, false);
    rc = storePel();
    fs::remove(fileName);
    return rc;
}

int PelHandler::storePel()
{
    static constexpr auto logObjPath = "/xyz/openbmc_project/logging";
    static constexpr auto logInterface = "xyz.openbmc_project.Logging.Create";

    static sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

    std::string service{};
    std::map<std::string, std::string> addlData{};
    try
    {
        service = getService(bus, logObjPath, logInterface);
    }

    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>("Falied to get d-bus service for handling a pel");
        return PLDM_ERROR;
    }

    using namespace sdbusplus::xyz::openbmc_project::Logging::server;
    addlData["RAWPEL"] = pelFileName;
    auto severity =
        sdbusplus::xyz::openbmc_project::Logging::server::convertForMessage(
            sdbusplus::xyz::openbmc_project::Logging::server::Entry::Level::
                Error);

    auto method = bus.new_method_call(service.c_str(), logObjPath, logInterface,
                                      "Create");
    method.append("xyz.openbmc_project.Host.Error.Event", severity, addlData);
    bus.call_noreply(method);

    return PLDM_SUCCESS;
}

} // namespace oem_file_type

} // namespace responder

} // namespace pldm
