#include "file_io_type_dump.hpp"

#include "utils.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <stdint.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include <exception>
#include <filesystem>
#include <iostream>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Dump/NewDump/server.hpp>

#include "libpldm/base.h"
#include "oem/ibm/libpldm/file_io.h"

using namespace pldm::utils;

namespace pldm
{
namespace responder
{

static constexpr auto nbdInterface = "/dev/nbd1";

int DumpHandler::fd = -1;

int DumpHandler::newFileAvailable(uint64_t length)
{
    std::cout << "length of dump is " << length
              << " and dump id= " << fileHandle << std::endl;

    static constexpr auto dumpObjPath = "/xyz/openbmc_project/dump";
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

int DumpHandler::writeFromMemory(uint32_t offset, uint32_t length,
                                 uint64_t address)
{
    int flags = O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE;

    if (DumpHandler::fd == -1)
    {
        DumpHandler::fd = open(nbdInterface, flags);
        if (DumpHandler::fd == -1)
        {
            std::cerr << "NBD file does not exist at " << nbdInterface
                      << " ERROR=" << errno << "\n";
            return PLDM_ERROR;
        }
    }
    return transferFileData(DumpHandler::fd, false, offset, length, address);
}

int DumpHandler::fileAck(uint8_t /*fileStatus*/)
{
    if (DumpHandler::fd >= 0)
    {
        static constexpr auto dumpObjPath = "/xyz/openbmc_project/dump";
        static constexpr auto MAPPER_BUSNAME =
            "xyz.openbmc_project.ObjectMapper";
        static constexpr auto MAPPER_PATH =
            "/xyz/openbmc_project/object_mapper";
        static constexpr auto MAPPER_INTERFACE =
            "xyz.openbmc_project.ObjectMapper";
        static constexpr auto systemDumpEntry =
            "xyz.openbmc_project.Dump.Entry.System";
        static constexpr auto dumpEntry = "xyz.openbmc_project.Dump.Entry";
        auto& bus = pldm::utils::DBusHandler::getBus();

        try
        {
            std::vector<std::string> paths;
            auto method =
                bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                    MAPPER_INTERFACE, "GetSubTreePaths");
            method.append(dumpObjPath);
            method.append(0);
            method.append(std::vector<std::string>({systemDumpEntry}));
            auto reply = bus.call(method);
            reply.read(paths);
            for (auto path : paths)
            {
                std::cout << "found dump path " << path << std::endl;
                uint32_t dumpId =
                    pldm::utils::DBusHandler().getDbusProperty<uint32_t>(
                        path.c_str(), "SourceDumpId", systemDumpEntry);
                if (dumpId == fileHandle)
                {
                    std::cout << "found dump id " << dumpId << std::endl;
                    PropertyValue value{true};
                    DBusMapping dbusMapping{path, dumpEntry, "Offloaded",
                                            "bool"};
                    pldm::utils::DBusHandler().setDbusProperty(dbusMapping,
                                                               value);
                    close(DumpHandler::fd);
                    DumpHandler::fd = -1;
                    return PLDM_SUCCESS;
                }
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "failed to make a d-bus call to DUMP manager, ERROR="
                      << e.what() << "\n";
        }
    }

    return PLDM_ERROR;
}

int DumpHandler::write(const char* buffer, uint32_t offset, uint32_t& length)
{
    int flags = O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE;
    if (DumpHandler::fd == -1)
    {
        DumpHandler::fd = open(nbdInterface, flags);
        if (DumpHandler::fd == -1)
        {
            std::cerr << "NBD file does not exist at " << nbdInterface
                      << " ERROR=" << errno << "\n";
            return PLDM_ERROR;
        }
    }

    int rc = lseek(DumpHandler::fd, offset, SEEK_SET);
    if (rc == -1)
    {
        std::cerr << "lseek failed, ERROR=" << errno << ", OFFSET=" << offset
                  << "\n";
        return PLDM_ERROR;
    }
    rc = ::write(DumpHandler::fd, buffer, length);
    if (rc == -1)
    {
        std::cerr << "file write failed, ERROR=" << errno
                  << ", LENGTH=" << length << ", OFFSET=" << offset << "\n";
        return PLDM_ERROR;
    }
    length = rc;

    return PLDM_SUCCESS;
}

} // namespace responder
} // namespace pldm
