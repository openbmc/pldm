#include "config.h"

#include "file_io_type_pel.hpp"

#include "utils.hpp"
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

int PelHandler::readIntoMemory(uint32_t offset, uint32_t& length,
                               uint64_t address)
{
    static constexpr auto logObjPath = "/xyz/openbmc_project/logging";
    static constexpr auto logInterface = "org.open_power.Logging.PEL";

    static sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

    try
    {
        auto service = pldm::utils::getService(bus, logObjPath, logInterface);
        auto method = bus.new_method_call(service.c_str(), logObjPath,
                                          logInterface, "GetPEL");
        method.append(fileHandle);
        auto reply = bus.call(method);
        sdbusplus::message::unix_fd fd{};
        reply.read(fd);
        auto rc = transferFileData(fd, true, offset, length, address);
        return rc;
    }
    catch (const std::exception& e)
    {
        std::cerr << "GetPEL D-Bus call failed, PEL id = " << fileHandle
                  << ", error = " << e.what() << "\n";
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

int PelHandler::read(uint32_t offset, uint32_t& length, Response& response)
{
    static constexpr auto logObjPath = "/xyz/openbmc_project/logging";
    static constexpr auto logInterface = "org.open_power.Logging.PEL";
    static sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

    try
    {
        auto service = pldm::utils::getService(bus, logObjPath, logInterface);
        auto method = bus.new_method_call(service.c_str(), logObjPath,
                                          logInterface, "GetPEL");
        method.append(fileHandle);
        auto reply = bus.call(method);
        sdbusplus::message::unix_fd fd{};
        reply.read(fd);
        std::cerr << "GetPEL D-Bus call done\n";
        off_t fileSize = lseek(fd, 0, SEEK_END);
        if (fileSize == -1)
        {
            std::cerr << "file seek failed";
            return PLDM_ERROR;
        }
        if (offset >= fileSize)
        {
            std::cerr << "Offset exceeds file size, OFFSET=" << offset
                      << " FILE_SIZE=" << fileSize << std::endl;
            return PLDM_DATA_OUT_OF_RANGE;
        }
        if (offset + length > fileSize)
        {
            length = fileSize - offset;
        }
        auto rc = lseek(fd, offset, SEEK_SET);
        if (rc == -1)
        {
            std::cerr << "file seek failed";
            return PLDM_ERROR;
        }
        size_t currSize = response.size();
        response.resize(currSize + length);
        auto filePos = reinterpret_cast<char*>(response.data());
        filePos += currSize;
        rc = ::read(fd, filePos, length);
        if (rc == -1)
        {
            std::cerr << "file read failed";
            return PLDM_ERROR;
        }
        if (rc != length)
        {
            std::cerr << "mismatch between number of characters to read and "
                      << "the length read, LENGTH=" << length << " COUNT=" << rc
                      << std::endl;
            return PLDM_ERROR;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "GetPEL D-Bus call failed";
        return PLDM_ERROR;
    }
    return PLDM_SUCCESS;
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

int PelHandler::fileAck(uint8_t /*fileStatus*/)
{
    static constexpr auto logObjPath = "/xyz/openbmc_project/logging";
    static constexpr auto logInterface = "org.open_power.Logging.PEL";
    static sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

    try
    {
        auto service = pldm::utils::getService(bus, logObjPath, logInterface);
        auto method = bus.new_method_call(service.c_str(), logObjPath,
                                          logInterface, "HostAck");
        method.append(fileHandle);
        bus.call_noreply(method);
    }
    catch (const std::exception& e)
    {
        std::cerr << "HostAck D-Bus call failed";
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

int PelHandler::storePel(std::string&& pelFileName)
{
    static constexpr auto logObjPath = "/xyz/openbmc_project/logging";
    static constexpr auto logInterface = "xyz.openbmc_project.Logging.Create";

    static sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

    try
    {
        auto service = pldm::utils::getService(bus, logObjPath, logInterface);
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
