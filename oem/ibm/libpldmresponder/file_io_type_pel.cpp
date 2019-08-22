#include "config.h"

#include "file_io_type_pel.hpp"

#include "libpldmresponder/utils.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

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
using namespace phosphor::logging;

namespace responder
{

namespace oem_file_type
{

int PelHandler::readIntoMemory()
{
    log<level::ERR>("Not supported for PELs");
    return PLDM_ERROR;
}

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

    auto rc = this->transferFileData(path, false);

    if (rc != PLDM_SUCCESS)
    {
        fs::remove(fileName);
        return rc;
    }
    rc = storePel(fileName);
    fs::remove(fileName);
    return rc;
}

int PelHandler::storePel(const std::string& pelFileName)
{
    static constexpr auto logObjPath = "/xyz/openbmc_project/logging";
    static constexpr auto logInterface = "xyz.openbmc_project.Logging.Create";

    static sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

    std::string service{};
    std::map<std::string, std::string> addlData{};
    try
    {
        service = getService(bus, logObjPath, logInterface);
        using namespace sdbusplus::xyz::openbmc_project::Logging::server;
        addlData["RAWPEL"] = pelFileName;
        auto severity =
            sdbusplus::xyz::openbmc_project::Logging::server::convertForMessage(
                sdbusplus::xyz::openbmc_project::Logging::server::Entry::Level::
                    Error);

        auto method = bus.new_method_call(service.c_str(), logObjPath,
                                          logInterface, "Create");
        method.append("xyz.openbmc_project.Host.Error.Event", severity,
                      addlData);
        bus.call_noreply(method);
    }

    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>("Falied to get d-bus service for handling a pel",
                        entry("ERROR=%s", e.what()));
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

} // namespace oem_file_type

} // namespace responder

} // namespace pldm
