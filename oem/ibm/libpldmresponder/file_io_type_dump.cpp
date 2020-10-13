#include "file_io_type_dump.hpp"

#include "libpldm/base.h"
#include "oem/ibm/libpldm/file_io.h"

#include "common/utils.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <stdint.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Dump/NewDump/server.hpp>

#include <exception>
#include <filesystem>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>


using namespace pldm::utils;

namespace pldm
{
namespace responder
{

static constexpr auto nbdInterfaceDefault = "/tmp/socketDemo";
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

    std::string nbdInterface{};
#if 0
    try
    {
        nbdInterface = pldm::utils::DBusHandler().getDbusProperty<std::string>(
            path.c_str(), "OffloadUri", dumpEntry);
    }
    catch (const std::exception& e)
    {
        std::cerr << "failed to make a d-bus call to DUMP manager, ERROR="
                  << e.what() << "\n";
    }

    if (nbdInterface == "")
    {
        nbdInterface = nbdInterfaceDefault;
    }
#endif
    nbdInterface = nbdInterfaceDefault;
    //std::cout << nbdInterface << std::endl; 

    return nbdInterface;
}

int DumpHandler::writeFromMemory(uint32_t offset, uint32_t length,
                                 uint64_t address)
{
    auto nbdInterface = getOffloadUri(fileHandle);

 if (DumpHandler::fd == -1)
 {
    int sock;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    char path[] = "/tmp/socketDemo";
    addr.sun_family = AF_UNIX;
    if (strnlen(path, sizeof(addr.sun_path)) == sizeof(addr.sun_path))
    {
        std::cerr << "UNIX socket path too long" << std::endl;
        return PLDM_ERROR;
    }

    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    //std::cout << addr.sun_path << std::endl;

    if ((sock = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1)
    {
        std::cerr << "SOCKET failed" << std::endl;
        return PLDM_ERROR;
    }

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        std::cerr << "CONNECT failed" << std::endl;
        close(sock);
        return PLDM_ERROR;
    }
    DumpHandler::fd = sock;
}

    return transferFileDataToSocket(DumpHandler::fd, false, offset, length, address);
}


bool writeonfd(const int sock,const char* buf,const uint64_t blockSize)
{
    uint64_t i;
    int nwrite=0;

    for(i =0; i< blockSize; i = i + nwrite)
    {

    fd_set wfd;
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    FD_ZERO(&wfd);
    FD_SET(sock, &wfd);
    int nfd = sock + 1;

        int retval = select(nfd, NULL, &wfd, NULL, &tv);
        if((retval < 0) && (errno!=EINTR))
        {
            std::cout << "\nselect call failed "<< errno << std::endl;
            return true;
        }
        if(retval == 0)
        {
           nwrite=0;
           continue;
        }
        if ((retval > 0) && (FD_ISSET(sock, &wfd)))
        {
            nwrite = write(sock, buf+i,blockSize-i);

            if (nwrite<0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno== EINTR)
                {
                  std::cout << "Write call failed with EAGAIN or EWOULDBLOCK or EINTR";
                  nwrite=0;
                  continue;
                }
                   std::cout << "Failed to write";
                   return true;
            }
         }
         else {
              nwrite = 0;
              std::cout << "fd is not ready for write";

         }
     }
     return false;
}

int DumpHandler::write(const char* buffer, uint32_t offset, uint32_t& length)
{

if(DumpHandler::fd == -1)
{
     int sock;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    char path[] = "/tmp/socketDemo";
    addr.sun_family = AF_UNIX;
    if (strnlen(path, sizeof(addr.sun_path)) == sizeof(addr.sun_path))
    {
        std::cerr << "UNIX socket path too long" << std::endl;
        return PLDM_ERROR;
    }

    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    //std::cout << addr.sun_path << std::endl;

    if ((sock = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1)
    {
        std::cerr << "SOCKET failed" << std::endl;
        return PLDM_ERROR;
    }

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        std::cerr << "CONNECT failed" << std::endl;
        close(sock);
        return PLDM_ERROR;
    }

    DumpHandler::fd = sock;
}

#if 0
    int rc = ::write(DumpHandler::fd, buffer, length);
    if (rc == -1)
    {
        std::cerr << "file write failed, ERROR=" << errno
                  << ", LENGTH=" << length << ", OFFSET=" << offset << "\n";
        return PLDM_ERROR;
    }
#endif
    bool status = writeonfd(DumpHandler::fd, buffer, length);
    if(status)
    {
         close(fd);
         std::cerr << "closing socket" << std::endl;
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
            DumpHandler::fd = -1;
            return PLDM_SUCCESS;
        }
    }

    return PLDM_ERROR;
}

} // namespace responder
} // namespace pldm
