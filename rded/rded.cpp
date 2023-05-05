#include "common/utils.hpp"

#include <locale.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <helper/mctpsetup.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/asio/property.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/unpack_properties.hpp>
#include <sdeventplus/event.hpp>

#include <cstddef>
#include <iostream>
#include <map>
#include <vector>

using namespace sdeventplus;
using namespace pldm::utils;

using DbusVariant = std::variant<std::string, uint64_t, uint32_t, uint16_t,
                                 uint8_t, sdbusplus::message::object_path>;
using ChangedPropertiesType = std::vector<std::pair<std::string, DbusVariant>>;

constexpr bool DEBUG_ENABLED = false;

int fd; // MCTP Socket for RDE communication

std::map<std::string, int> deviceNetIdMap;
std::map<std::string, std::shared_ptr<sdbusplus::asio::dbus_interface>>
    dbusIntfMap;

int triggerRdeReactor(int fd)
{
    /**
     * RDE Reactor is responsible to react to a new RDE device whenever Entity
     * Manager adds a new object path in its service tree that has the interface
     * "xyz.openbmc_project.Configuration.RdeSatelliteController"
     *
     * The detector code for detecting a RDE Device resides in Entity Manager
     */
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus =
        std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objectServer(systemBus, true);
    objectServer.add_manager("/xyz/openbmc_project/rde_devices");
    std::string prefixPath = "/xyz/openbmc_project/rde_devices/";

    std::cerr << "Beginning RDE reactor...\n";

    // TODO(@kkachana,@harshtya): Add the enumeration code for already existing
    // RDE devices in the Entity Manager service tree

    std::make_unique<sdbusplus::bus::match_t>(
        *systemBus, sdbusplus::bus::match::rules::interfacesAdded(),
        [&objectServer, &prefixPath, &systemBus,
         &fd](sdbusplus::message_t& reply) {
            sdbusplus::message::object_path changedObject;
            reply.read(changedObject);
            std::vector<std::pair<std::string, ChangedPropertiesType>>
                changedInterfaces;
            reply.read(changedInterfaces);

            std::string vendorId;
            std::string udevid;
            std::string port;
            for (const auto& [changedInterface, changedProps] :
                 changedInterfaces)
            {
                if (changedInterface !=
                    "xyz.openbmc_project.Configuration.RdeSatelliteController")
                {
                    continue;
                }

                if (DEBUG_ENABLED)
                {
                    std::cerr << "DEBUG: Changed object path: "
                              << std::string(changedObject) << "\n";
                }

                for (auto& [key, value] : changedProps)
                {
                    if (key == "VID")
                    {
                        vendorId = std::get<std::string>(value);
                    }
                    else if (key == "USBPORT")
                    {
                        port = std::get<std::string>(value);
                    }
                    else if (key == "UDEVID")
                    {
                        udevid = std::get<std::string>(value);
                    }
                }
                if (vendorId.empty() || port.empty() || udevid.empty())
                {
                    std::cerr << "Matcher does not have required properties\n";
                    return;
                }

                int netId = setupOnePort(port, udevid); // MCTP Setup
                deviceNetIdMap.insert({udevid, netId});

                // TODO(@harshtya): Initiate PLDM and discovery

                std::string objectPath = prefixPath + udevid;
                if (DEBUG_ENABLED)
                {
                    std::cerr << "New object path created: " << objectPath
                              << "\n";
                }

                std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
                    objectServer.add_interface(objectPath,
                                               "xyz.openbmc_project.RdeDevice");
                iface->register_property(
                    "VID", vendorId,
                    sdbusplus::asio::PropertyPermission::readOnly);
                iface->register_property(
                    "USBPORT", port,
                    sdbusplus::asio::PropertyPermission::readOnly);
                iface->register_property(
                    "UDEVID", udevid,
                    sdbusplus::asio::PropertyPermission::readOnly);

                // TODO(@harshtya): Add method to handle RDE operation requests

                iface->initialize();
                dbusIntfMap.insert({std::string(changedObject), iface});
            }
        });

    // Remove object path if the device is removed
    std::make_unique<sdbusplus::bus::match_t>(
        *systemBus, sdbusplus::bus::match::rules::interfacesRemoved(),
        [&objectServer](sdbusplus::message_t& reply) {
            sdbusplus::message::object_path changedObject;
            std::vector<std::string> interfacesRemoved;
            reply.read(changedObject, interfacesRemoved);

            auto it = dbusIntfMap.find(std::string(changedObject));
            if (it != dbusIntfMap.end())
            {
                std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
                    it->second;
                auto removed = objectServer.remove_interface(iface);
                std::cout << "Removed RDE Device from tree: " << removed
                          << "\n";
                dbusIntfMap.erase(std::string(changedObject));
                // TODO (@harshtya): clean up the manager for this object path
                // TODO (@harshtya): clean up the dictionaries
            }
            return;
        });

    systemBus->request_name("xyz.openbmc_project.rdeoperation");
    io.run();
    return 0;
}

void setSocketTimeout(int fd, int seconds, int milliseconds)
{
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = milliseconds;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
}

int main()
{
    int rc;
    std::cerr << "Successfully started the RDE Daemon \n";
    fd = socket(AF_MCTP, SOCK_DGRAM, 0);
    if (-1 == fd)
    {
        std::cerr << "Created socket Failed\n";
        return fd;
    }

    std::cerr << "Socket Created with ID: " << fd << ". Setting timeouts...\n";
    setSocketTimeout(fd, /*seconds=*/5, /*milliseconds=*/0);
    socklen_t optlen;
    int currentSendbuffSize;
    optlen = sizeof(currentSendbuffSize);

    int res =
        getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &currentSendbuffSize, &optlen);
    if (res == -1)
    {
        std::cerr << "Error in obtaining the default send buffer size, Error: "
                  << strerror(errno) << std::endl;
        return -1;
    }

    // TODO: Use PDR P&M PLDM Type to create Resource id mapping

    rc = triggerRdeReactor(fd);
    if (rc)
    {
        std::cerr << "Error in RDE reactor \n";
        return rc;
    }

    std::cerr << "RDE Reactor stopped...\n";
    // Daemon loop -- Code never reaches here if the reactor is running fine--
    auto event = Event::get_default();
    rc = event.loop();
    if (rc)
    {
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}
