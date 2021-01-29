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
#include <type_traits>

using namespace pldm::responder::utils;
using namespace pldm::utils;

namespace pldm
{
namespace responder
{

static constexpr auto dumpEntry = "xyz.openbmc_project.Dump.Entry";
static constexpr auto dumpObjPath = "/xyz/openbmc_project/dump/system";
static constexpr auto systemDumpEntry = "xyz.openbmc_project.Dump.Entry.System";
static constexpr auto resDumpObjPath = "/xyz/openbmc_project/dump/resource";
static constexpr auto resDumpDirPath = "/var/lib/pldm/resourcedump/1";
static constexpr auto resDumpEntry = "com.ibm.Dump.Entry.Resource";
int DumpHandler::fd = -1;

std::string DumpHandler::findDumpObjPath(uint32_t fileHandle)
{
    static constexpr auto MAPPER_BUSNAME = "xyz.openbmc_project.ObjectMapper";
    static constexpr auto MAPPER_PATH = "/xyz/openbmc_project/object_mapper";
    static constexpr auto MAPPER_INTERFACE = "xyz.openbmc_project.ObjectMapper";
    auto& bus = pldm::utils::DBusHandler::getBus();

    try
    {
        std::vector<std::string> paths;
        auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                          MAPPER_INTERFACE, "GetSubTreePaths");
        if (dumpType == PLDM_FILE_TYPE_DUMP)
        {
            method.append(dumpObjPath);
        }
        else if ((dumpType == PLDM_FILE_TYPE_RESOURCE_DUMP) ||
                 (dumpType == PLDM_FILE_TYPE_RESOURCE_DUMP_PARMS))
        {
            method.append(resDumpObjPath);
        }

        method.append(0);

        if (dumpType == PLDM_FILE_TYPE_DUMP)
        {
            method.append(std::vector<std::string>({systemDumpEntry}));
        }
        else if ((dumpType == PLDM_FILE_TYPE_RESOURCE_DUMP) ||
                 (dumpType == PLDM_FILE_TYPE_RESOURCE_DUMP_PARMS))
        {
            method.append(std::vector<std::string>({resDumpEntry}));
        }

        auto reply = bus.call(method);
        reply.read(paths);

        // Stores the current resource dump entry path
        std::string curResDumpEntryPath;

        for (const auto& path : paths)
        {
            uint32_t dumpId = 0;
            if (dumpType == PLDM_FILE_TYPE_DUMP)
            {
                dumpId = pldm::utils::DBusHandler().getDbusProperty<uint32_t>(
                    path.c_str(), "SourceDumpId", systemDumpEntry);
            }
            else if ((dumpType == PLDM_FILE_TYPE_RESOURCE_DUMP) ||
                     (dumpType == PLDM_FILE_TYPE_RESOURCE_DUMP_PARMS))
            {
                dumpId = pldm::utils::DBusHandler().getDbusProperty<uint32_t>(
                    path.c_str(), "SourceDumpId", resDumpEntry);
            }

            if (dumpId == fileHandle)
            {
                std::cout << "Matched DumpHandler::findDumpObjPath dumpid ="
                          << dumpId << "fileHandle=" << fileHandle << "\n";
                return path;
            }
            curResDumpEntryPath = path;
        }

        // Need to return from here as PHYP is not setting the dump id
        // during the File Ack command
        if ((dumpType == PLDM_FILE_TYPE_RESOURCE_DUMP_PARMS) ||
            (dumpType == PLDM_FILE_TYPE_RESOURCE_DUMP))
        {
            std::cout << "Matched DumpHandler::findDumpObjPath dumpType ="
                      << dumpType << "fileHandle=" << fileHandle << "\n";
            return curResDumpEntryPath;
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

    auto notifyObjPath = dumpObjPath;
    if (dumpType == PLDM_FILE_TYPE_RESOURCE_DUMP)
    {
        // Setting the Notify path for resource dump
        notifyObjPath = resDumpObjPath;

        uint32_t sourceDumpId = fileHandle;
        std::cout << "DumpHandler::newFileAvailable sourceDumpId ="
                  << sourceDumpId << "\n";
        auto path = findDumpObjPath(fileHandle);

        pldm::utils::PropertyValue propValue{sourceDumpId};

        DBusMapping dbusMapping{path, resDumpEntry, "SourceDumpId", "uint32_t"};
        try
        {
            pldm::utils::DBusHandler().setDbusProperty(dbusMapping, propValue);
        }
        catch (const std::exception& e)
        {
            std::cerr << "failed to make a d-bus call to DUMP manager to set "
                         "resource dump SourceDumpId, ERROR="
                      << e.what() << "\n";
        }
    }

    try
    {
        auto service =
            pldm::utils::DBusHandler().getService(notifyObjPath, dumpInterface);
        using namespace sdbusplus::xyz::openbmc_project::Dump::server;
        auto method = bus.new_method_call(service.c_str(), notifyObjPath,
                                          dumpInterface, "Notify");
        method.append(fileHandle, length);
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

std::string DumpHandler::getOffloadUri(uint32_t fileHandle)
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

int DumpHandler::writeFromMemory(uint32_t, uint32_t length, uint64_t address,
                                 oem_platform::Handler* /*oemPlatformHandler*/)
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
    return transferFileDataToSocket(DumpHandler::fd, length, address);
}

int DumpHandler::write(const char* buffer, uint32_t, uint32_t& length,
                       oem_platform::Handler* /*oemPlatformHandler*/)
{
    int rc = writeToUnixSocket(DumpHandler::fd, buffer, length);
    if (rc < 0)
    {
        rc = -errno;
        close(DumpHandler::fd);
        auto socketInterface = getOffloadUri(fileHandle);
        std::remove(socketInterface.c_str());
        std::cerr << "DumpHandler::write: writeToUnixSocket() failed"
                  << std::endl;
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

int DumpHandler::fileAck(uint8_t fileStatus)
{
    auto path = findDumpObjPath(fileHandle);
    if (dumpType == PLDM_FILE_TYPE_RESOURCE_DUMP_PARMS)
    {
        if (fileStatus != PLDM_SUCCESS)
        {
            std::cerr << "Failue in resource dump file ack" << std::endl;
            pldm::utils::reportError(
                "xyz.openbmc_project.bmc.pldm.InternalFailure");

            PropertyValue value{
                "xyz.openbmc_project.Common.Progress.OperationStatus.Failed"};
            DBusMapping dbusMapping{path, "xyz.openbmc_project.Common.Progress",
                                    "Status", "string"};
            try
            {
                pldm::utils::DBusHandler().setDbusProperty(dbusMapping, value);
            }
            catch (const std::exception& e)
            {
                std::cerr << "failed to make a d-bus call to DUMP "
                             "manager, ERROR="
                          << e.what() << "\n";
            }
        }

        namespace fs = std::filesystem;
        if (fs::exists(resDumpDirPath))
        {
            fs::remove_all(resDumpDirPath);
            std::cout << "Remove resource dump param file" << resDumpDirPath
                      << "\n";
        }
        return PLDM_SUCCESS;
    }

    if (DumpHandler::fd >= 0)
    {
        if (!path.empty())
        {
            if ((dumpType == PLDM_FILE_TYPE_DUMP) ||
                (dumpType == PLDM_FILE_TYPE_RESOURCE_DUMP))
            {
                PropertyValue value{true};
                DBusMapping dbusMapping{path, dumpEntry, "Offloaded", "bool"};
                try
                {
                    pldm::utils::DBusHandler().setDbusProperty(dbusMapping,
                                                               value);
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
            }
            return PLDM_SUCCESS;
        }
    }

    return PLDM_ERROR;
}

int DumpHandler::readIntoMemory(uint32_t offset, uint32_t& length,
                                uint64_t address,
                                oem_platform::Handler* /*oemPlatformHandler*/)
{
    if (dumpType != PLDM_FILE_TYPE_RESOURCE_DUMP_PARMS)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }
    else
    {
        return transferFileData(resDumpDirPath, true, offset, length, address);
    }
}

int DumpHandler::read(uint32_t offset, uint32_t& length, Response& response,
                      oem_platform::Handler* /*oemPlatformHandler*/)
{
    if (dumpType != PLDM_FILE_TYPE_RESOURCE_DUMP_PARMS)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }
    else
    {
        return readFile(resDumpDirPath, offset, length, response);
    }
}

} // namespace responder
} // namespace pldm
