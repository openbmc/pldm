#include "utils.hpp"

#include "libpldm/base.h"
#include "common/utils.hpp"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <iostream>
#include <exception>

namespace pldm
{
namespace responder
{
namespace utils
{

int setupUnixSocket(const std::string& socketInterface)
{
    int sock;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (strnlen(socketInterface.c_str(), sizeof(addr.sun_path)) ==
        sizeof(addr.sun_path))
    {
        std::cerr << "setupUnixSocket: UNIX socket path too long" << std::endl;
        return -1;
    }

    strncpy(addr.sun_path, socketInterface.c_str(), sizeof(addr.sun_path) - 1);

    if ((sock = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1)
    {
        std::cerr << "setupUnixSocket: socket() call failed" << std::endl;
        return -1;
    }

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        std::cerr << "setupUnixSocket: bind() call failed" << std::endl;
        close(sock);
        return -1;
    }

    if (listen(sock, 1) == -1)
    {
        std::cerr << "setupUnixSocket: listen() call failed" << std::endl;
        close(sock);
        return -1;
    }

    fd_set rfd;
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    FD_ZERO(&rfd);
    FD_SET(sock, &rfd);
    int nfd = sock + 1;
    int fd = -1;

    int retval = select(nfd, &rfd, NULL, NULL, &tv);
    if (retval < 0)
    {
        std::cerr << "setupUnixSocket: select call failed " << errno
                  << std::endl;
        close(sock);
        return -1;
    }

    if ((retval > 0) && (FD_ISSET(sock, &rfd)))
    {
        fd = accept(sock, NULL, NULL);
        if (fd < 0)
        {
            std::cerr << "setupUnixSocket: accept() call failed " << errno
                      << std::endl;
            close(sock);
            return -1;
        }
        close(sock);
    }
    return fd;
}

int writeToUnixSocket(const int sock, const char* buf, const uint64_t blockSize)
{
    uint64_t i;
    int nwrite = 0;

    for (i = 0; i < blockSize; i = i + nwrite)
    {

        fd_set wfd;
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        FD_ZERO(&wfd);
        FD_SET(sock, &wfd);
        int nfd = sock + 1;

        int retval = select(nfd, NULL, &wfd, NULL, &tv);
        if (retval < 0)
        {
            std::cerr << "writeToUnixSocket: select call failed " << errno
                      << std::endl;
            close(sock);
            return -1;
        }
        if (retval == 0)
        {
            nwrite = 0;
            continue;
        }
        if ((retval > 0) && (FD_ISSET(sock, &wfd)))
        {
            nwrite = write(sock, buf + i, blockSize - i);
            if (nwrite < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                {
                    std::cerr << "writeToUnixSocket: Write call failed with "
                                 "EAGAIN or EWOULDBLOCK or EINTR"
                              << std::endl;
                    nwrite = 0;
                    continue;
                }
                std::cerr << "writeToUnixSocket: Failed to write" << std::endl;
                close(sock);
                return -1;
            }
        }
        else
        {
            nwrite = 0;
        }
    }
    return 0;
}

bool checkIfIBMCableCard(const std::string& objPath)
{
   constexpr auto pcieAdapterModelInterface = "xyz.openbmc_project.Inventory.Decorator.Asset"; 
   constexpr auto modelProperty = "Model";

   try
   {
       auto propVal = pldm::utils::DBusHandler().getDbusPropertyVariant(
           objPath.c_str(), modelProperty, pcieAdapterModelInterface);
       const auto& model = std::get<std::string>(propVal);
       if(!model.empty())
       {
           return true;
       }
   }
   catch (const sdbusplus::exception::SdBusError& e)
   {
       return false;
   }
   return false;
}


void findPortObjects(const std::string& cardObjPath, std::vector<std::string>& portObjects)
{
    std::cout << "entered findPortObjects with " << cardObjPath << "\n";
    static constexpr auto MAPPER_BUSNAME = "xyz.openbmc_project.ObjectMapper";
    static constexpr auto MAPPER_PATH = "/xyz/openbmc_project/object_mapper";
    static constexpr auto MAPPER_INTERFACE = "xyz.openbmc_project.ObjectMapper";
    static constexpr auto portInterface = "xyz.openbmc_project.Inventory.Item.Connector";

    auto& bus = pldm::utils::DBusHandler::getBus();
    try
    {
        auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                          MAPPER_INTERFACE, "GetSubTreePaths");
        method.append(cardObjPath);
        method.append(0);
        method.append(std::vector<std::string>({portInterface}));
        auto reply = bus.call(method);
        reply.read(portObjects);
    }
    catch (const std::exception& e)
    {
        std::cerr << "no ports under card " << cardObjPath << "\n";
    }
    std::cout << "\nexit findPortObjects \n";
}

bool checkFruPresence(const char* objPath)
{
    std::string nvme("nvme");
    std::string pcieAdapter("pcie_cable_card");
    //if we enter here with port objects then we need to find the parent
    //and see if the card is present. if so then the port is considered as
    //present. this is so because the ports do not have "Present" property
    bool isPresent = true;
    std::string portStr("cxp_");
    std::string newObjPath = objPath;
    if(newObjPath.find(nvme) != std::string::npos)
    {
        std::cout << "nvme card return true\n";
        return true;
    }
    else if((newObjPath.find(pcieAdapter)!= std::string::npos)&& !checkIfIBMCableCard(newObjPath))
    {
        std::cout << "return true for industry std cards \n";
        return true;
    }
    else if(newObjPath.find(portStr) != std::string::npos)
    {
        newObjPath = pldm::utils::findParent(objPath);
    }

    //we need to return true for industry std pcie cards and nvme cards
    //even if the Present is false
    
    static constexpr auto presentInterface =
        "xyz.openbmc_project.Inventory.Item";
    static constexpr auto presentProperty = "Present";
    try
    {
        auto propVal = pldm::utils::DBusHandler().getDbusPropertyVariant(
            newObjPath.c_str(), presentProperty, presentInterface);
        isPresent = std::get<bool>(propVal);
        std::cout << "\nfound isPresent as " <<(uint32_t) isPresent;
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        std::cerr << "object " << objPath << " does not implement Presence";
    }
    std::cout << "\n returning isPresent as " << (uint32_t)isPresent
        << " for object " << objPath << "\n";
    return isPresent;    
}

} // namespace utils
} // namespace responder
} // namespace pldm
