#include "file_io_type_dump.hpp"

#include "libpldm/base.h"
#include "oem/ibm/libpldm/file_io.h"

#include "common/utils.hpp"
#include "utils.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <stdint.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Dump/NewDump/server.hpp>

#include <exception>
#include <filesystem>
#include <iostream>

using namespace pldm::responder::utils;
using namespace pldm::utils;

namespace pldm
{
namespace responder
{

static constexpr auto dumpEntry = "xyz.openbmc_project.Dump.Entry";
static constexpr auto dumpObjPath = "/xyz/openbmc_project/dump/system";
int DumpHandler::fd = -1;

static std::string findDumpObjPath(uint32_t fileHandle)
{
    static constexpr auto MAPPER_BUSNAME = "xyz.openbmc_project.ObjectMapper";
    static constexpr auto MAPPER_PATH = "/xyz/openbmc_project/object_mapper";
    static constexpr auto MAPPER_INTERFACE = "xyz.openbmc_project.ObjectMapper";
    static constexpr auto systemDumpEntry =
        "xyz.openbmc_project.Dump.Entry.System";
    auto& bus = pldm::utils::DBusHandler::getBus();

    try
    {
        std::vector<std::string> paths;
        auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                          MAPPER_INTERFACE, "GetSubTreePaths");
        method.append(dumpObjPath);
        method.append(0);
        method.append(std::vector<std::string>({systemDumpEntry}));
        auto reply = bus.call(method);
        reply.read(paths);
        for (const auto& path : paths)
        {
            auto dumpId = pldm::utils::DBusHandler().getDbusProperty<uint32_t>(
                path.c_str(), "SourceDumpId", systemDumpEntry);
            if (dumpId == fileHandle)
            {
                return path;
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "failed to make a d-bus call to DUMP manager, ERROR="
                  << e.what() << "\n";
    }

    std::cerr << "failed to find dump object for dump id " << fileHandle
              << "\n";
    return {};
}

int DumpHandler::newFileAvailable(uint64_t length)
{
    static constexpr auto dumpInterface = "xyz.openbmc_project.Dump.NewDump";
    auto& bus = pldm::utils::DBusHandler::getBus();

    try
    {
        auto service =
            pldm::utils::DBusHandler().getService(dumpObjPath, dumpInterface);
        using namespace sdbusplus::xyz::openbmc_project::Dump::server;
        auto method = bus.new_method_call(service.c_str(), dumpObjPath,
                                          dumpInterface, "Notify");
        method.append(
            sdbusplus::xyz::openbmc_project::Dump::server::convertForMessage(
                NewDump::DumpType::System),
            fileHandle, length);
        bus.call_noreply(method);
    }
    catch (const std::exception& e)
    {
        std::cerr << "failed to make a d-bus call to DUMP manager, ERROR="
                  << e.what() << "\n";
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

static std::string getOffloadUri(uint32_t fileHandle)
{
    auto path = findDumpObjPath(fileHandle);
    if (path.empty())
    {
        return {};
    }

    std::string socketInterface{};

    try
    {
        socketInterface =
            pldm::utils::DBusHandler().getDbusProperty<std::string>(
                path.c_str(), "OffloadUri", dumpEntry);
    }
    catch (const std::exception& e)
    {
        std::cerr << "failed to make a d-bus call to DUMP manager, ERROR="
                  << e.what() << "\n";
    }

    return socketInterface;
}

int DumpHandler::writeFromMemory(uint32_t offset, uint32_t length,
                                 uint64_t address)
{
    if (DumpHandler::fd == -1)
    {
        auto socketInterface = getOffloadUri(fileHandle);
        int sock = setupUnixSocket(socketInterface);
        if (sock < 0)
        {
            sock = -errno;
            close(DumpHandler::fd);
            std::cerr
                << "DumpHandler::writeFromMemory: setupUnixSocket() failed"
                << std::endl;
            std::remove(socketInterface.c_str());
            return PLDM_ERROR;
        }

        DumpHandler::fd = sock;
    }
    return transferFileDataToSocket(DumpHandler::fd, false, offset, length,
                                    address);
}

int DumpHandler::write(const char* buffer, uint32_t, uint32_t& length)
{
    int rc = writeOnUnixSocket(DumpHandler::fd, buffer, length);
    if (rc < 0)
    {
        rc = -errno;
        close(DumpHandler::fd);
        auto socketInterface = getOffloadUri(fileHandle);
        std::remove(socketInterface.c_str());
        std::cerr << "DumpHandler::write: writeOnUnixSocket() failed"
                  << std::endl;
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

int DumpHandler::fileAck(uint8_t /*fileStatus*/)
{
    if (DumpHandler::fd >= 0)
    {
        auto path = findDumpObjPath(fileHandle);
        if (!path.empty())
        {
            PropertyValue value{true};
            DBusMapping dbusMapping{path, dumpEntry, "Offloaded", "bool"};
            try
            {
                pldm::utils::DBusHandler().setDbusProperty(dbusMapping, value);
            }
            catch (const std::exception& e)
            {
                std::cerr
                    << "failed to make a d-bus call to DUMP manager, ERROR="
                    << e.what() << "\n";
            }

            close(DumpHandler::fd);
            auto socketInterface = getOffloadUri(fileHandle);
            std::remove(socketInterface.c_str());
            DumpHandler::fd = -1;
            return PLDM_SUCCESS;
        }
    }

    return PLDM_ERROR;
}

} // namespace responder
} // namespace pldm
