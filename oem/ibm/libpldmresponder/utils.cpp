#include "utils.hpp"

#include <libpldm/base.h>
#include <libpldm/platform.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/client.hpp>
#include <xyz/openbmc_project/Inventory/Item/Connector/client.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>

PHOSPHOR_LOG2_USING;

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
        error("setupUnixSocket: UNIX socket path too long");
        return -1;
    }

    strncpy(addr.sun_path, socketInterface.c_str(), sizeof(addr.sun_path) - 1);

    if ((sock = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1)
    {
        error("setupUnixSocket: socket() call failed");
        return -1;
    }

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        error("setupUnixSocket: bind() call failed  with errno {ERR}", "ERR",
              errno);
        close(sock);
        return -1;
    }

    if (listen(sock, 1) == -1)
    {
        error("setupUnixSocket: listen() call failed");
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
        error("setupUnixSocket: select call failed {ERR}", "ERR", errno);
        close(sock);
        return -1;
    }

    if ((retval > 0) && (FD_ISSET(sock, &rfd)))
    {
        fd = accept(sock, NULL, NULL);
        if (fd < 0)
        {
            error("setupUnixSocket: accept() call failed {ERR}", "ERR", errno);
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
            error("writeToUnixSocket: select call failed {ERR}", "ERR", errno);
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
                    error(
                        "writeToUnixSocket: Write call failed with EAGAIN or EWOULDBLOCK or EINTR");
                    nwrite = 0;
                    continue;
                }
                error("writeToUnixSocket: Failed to write {ERR}", "ERR", errno);
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

bool checkIfIBMFru(const std::string& objPath)
{
    using DecoratorAsset =
        sdbusplus::client::xyz::openbmc_project::inventory::decorator::Asset<>;

    try
    {
        auto propVal = pldm::utils::DBusHandler().getDbusPropertyVariant(
            objPath.c_str(), "Model", DecoratorAsset::interface);
        const auto& model = std::get<std::string>(propVal);
        if (!model.empty())
        {
            return true;
        }
    }
    catch (const std::exception&)
    {
        return false;
    }
    return false;
}

std::vector<std::string> findPortObjects(const std::string& adapterObjPath)
{
    using ItemConnector =
        sdbusplus::client::xyz::openbmc_project::inventory::item::Connector<>;

    std::vector<std::string> portObjects;
    try
    {
        portObjects = pldm::utils::DBusHandler().getSubTreePaths(
            adapterObjPath, 0,
            std::vector<std::string>({ItemConnector::interface}));
    }
    catch (const std::exception& e)
    {
        error("No ports under adapter '{ADAPTER_OBJ_PATH}'  - {ERROR}.",
              "ADAPTER_OBJ_PATH", adapterObjPath.c_str(), "ERROR", e);
    }

    return portObjects;
}

} // namespace utils

namespace oem_ibm_utils
{

void pldm::responder::oem_ibm_utils::Handler::setCoreCount(
    const EntityAssociations& Associations, EntityMaps entityMaps)
{
    static constexpr auto searchpath = "/xyz/openbmc_project/";
    std::vector<std::string> cpuInterface = {
        "xyz.openbmc_project.Inventory.Item.Cpu"};
    pldm::utils::GetSubTreeResponse response =
        pldm::utils::DBusHandler().getSubtree(searchpath, 0 /* depth */,
                                              cpuInterface);

    // get the CPU pldm entities
    for (const auto& entries : Associations)
    {
        auto parent = pldm_entity_extract(entries[0]);
        // entries[0] would be the parent in the entity association map
        if (parent.entity_type == PLDM_ENTITY_PROC)
        {
            int corecount = 0;
            for (const auto& entry : entries)
            {
                auto child = pldm_entity_extract(entry);
                if (child.entity_type == (PLDM_ENTITY_PROC | 0x8000))
                {
                    // got a core child
                    ++corecount;
                }
            }

            auto grand_parent = pldm_entity_get_parent(entries[0]);
            std::string grepWord =
                entityMaps.at(grand_parent.entity_type) +
                std::to_string(grand_parent.entity_instance_num) + "/" +
                entityMaps.at(parent.entity_type) +
                std::to_string(parent.entity_instance_num);
            for (const auto& [objectPath, serviceMap] : response)
            {
                // find the object path with first occurance of coreX
                if (objectPath.find(grepWord) != std::string::npos)
                {
                    pldm::utils::DBusMapping dbusMapping{
                        objectPath, cpuInterface[0], "CoreCount", "uint16_t"};
                    pldm::utils::PropertyValue value =
                        static_cast<uint16_t>(corecount);
                    try
                    {
                        pldm::utils::DBusHandler().setDbusProperty(dbusMapping,
                                                                   value);
                    }
                    catch (const std::exception& e)
                    {
                        error("Failed to set the core count property: {ERROR}",
                              "ERROR", e);
                    }
                }
            }
        }
    }
}

} // namespace oem_ibm_utils
} // namespace responder
} // namespace pldm
