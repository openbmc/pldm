#include "config.h"

#include "file_io_type_pel.hpp"

#include "libpldmresponder/utils.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <stdint.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include <exception>
#include <filesystem>
#include <iostream>
#include <sdbusplus/server.hpp>
#include <vector>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include "libpldm/base.h"
#include "oem/ibm/libpldm/file_io.h"

namespace pldm
{
namespace responder
{

int PelHandler::readIntoMemory(uint32_t /*offset*/, uint32_t& /*length*/,
                               uint64_t /*address*/)
{
    return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
}

int PelHandler::read(uint32_t /*offset*/, uint32_t& /*length*/,
                     Response& /*response*/)
{
    return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
}

int PelHandler::writeFromMemory(uint32_t offset, uint32_t length,
                                uint64_t address)
{
    char tmpFile[] = "/tmp/pel.XXXXXX";
    int fd = mkstemp(tmpFile);
    if (fd == -1)
    {
        std::cerr << "failed to create a temporary pel, ERROR=" << errno
                  << "\n";
        return PLDM_ERROR;
    }
    close(fd);
    fs::path path(tmpFile);

    auto rc = transferFileData(path, false, offset, length, address);
    if (rc == PLDM_SUCCESS)
    {
        rc = storePel(path.string());
    }
    fs::remove(path);
    return rc;
}

int PelHandler::storePel(std::string&& pelFileName)
{
    static constexpr auto logObjPath = "/xyz/openbmc_project/logging";
    static constexpr auto logInterface = "xyz.openbmc_project.Logging.Create";

    static sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

    try
    {
        auto service = getService(bus, logObjPath, logInterface);
        using namespace sdbusplus::xyz::openbmc_project::Logging::server;
        std::map<std::string, std::string> addlData{};
        addlData.emplace("RAWPEL", std::move(pelFileName));
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
    catch (const std::exception& e)
    {
        std::cerr << "failed to make a d-bus call to PEL daemon, ERROR="
                  << e.what() << "\n";
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

} // namespace responder
} // namespace pldm
