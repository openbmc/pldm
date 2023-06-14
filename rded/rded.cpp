#include "common/utils.hpp"
#include "helper/common.hpp"
#include "helper/discovery/base_discovery.hpp"
#include "helper/discovery/rde_discovery.hpp"
#include "helper/rde_operation/rde_operation.hpp"
#include "helper/resource_id_mapper.hpp"

//libbej
#include "libbej/bej_common.h"
#include "libbej/bej_encoder_core.h"
#include "libbej/bej_encoder_json.hpp"
#include <libbej/bej_decoder_json.hpp>

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
constexpr uint8_t DEST_EID = 9;
// TODO(@harshtya): Change instance id to be dynamic
constexpr uint8_t INSTANCE_ID = 1;

int fd; // MCTP Socket for RDE communication

boost::asio::io_context io;
std::map<std::string, int> deviceNetIdMap;
std::map<std::string, std::shared_ptr<sdbusplus::asio::dbus_interface>>
    dbusIntfMap;
std::map<int, int> retryCounterMap;
bool ready = true;

int initiateDiscovery(int fd, std::string udevId, int netId,
                       uint8_t destEid, uint8_t instanceId)
{
    int rc;

    // Begin Base Discovery
    std::cerr << "Initiating PLDM Discovery...\n";
    rc = performBaseDiscovery(udevId, fd, netId, destEid, instanceId);
    if (rc)
    {
        std::cerr << "Failure in Base Discovery with error code: " << rc
                  << "\n";
        return rc;
    }

    std::cerr << "Initializing RDE Discovery...\n";
    rc = performRdeDiscovery(udevId, fd, netId, destEid, instanceId);

    if (rc)
    {
        std::cerr << "Failure in Base Discovery with error code: " << rc
                  << "\n";
        return rc;
    }

    rc = performDictionaryDiscoveryForDevice(udevId, fd, destEid,
                                                 instanceId);

    if (rc)
    {
        std::cerr << "Failure in getting dictionaries with error code: " << rc
                  << "\n";
        return rc;
    }
    return 0;
}

/**
 * @brief generate operation id
*/
uint16_t generateOperationId()
{
    // TODO(@harshtya): Create an operation id generator (Following the spec)
    return 32770;
}
/**
 * @brief Retry counter tracker
*/
int getCurrentRetryCount(int requestId)
{
    auto it = retryCounterMap.find(requestId);
    if (it == retryCounterMap.end())
    {
        retryCounterMap.insert({requestId, 1});
        return 1;
    }

    int retryCount = it->second;
    retryCounterMap.insert({requestId, retryCount + 1});
    return retryCount + 1;
}
/**
 * @brief Removes the query parameters as we do not support expand yet
*/
int removeQueryParams(std::string* uri, std::string_view delimeter)
{
    std::string::size_type start = (*uri).find(delimeter);
    int end = (*uri).length();
    std::cout << end << "\n";
    if (start == std::string::npos)
    {
        std::cerr << "No query parameters found\n";
        return 1;
    }
    (*uri).erase(start, end - start + 1);
    return 0;
}
/**
 * @brief Checks whether the POST request is an UPDATE request or ACTION
*/
bool isActionOperation(std::string uri, std::string* resourceUri)
{
    std::string actionString = "/Actions";
    size_t index;
    if ((index = uri.find(actionString)) != std::string::npos)
    {
        *resourceUri = uri.substr(0, index);
        // std::cerr << "Resource uri: " << *resource_uri << "\n";
        return true;
    }
    std::cerr << "Not found\n";
    return false;
}
/**
 * Translate BEJ to string
*/
int translateBejToString(uint32_t resourceId, std::string deviceId,
                            std::string* json, uint8_t** payload,
                            uint32_t payloadLength)
{
    int rc;
    uint32_t annotationDictLen;
    uint8_t* annotationDict;

    // get_dictioanry_for_rid from particular resource id
    rc = getDictionaryForRidDev(deviceId, 0xffffffff, &annotationDict,
                                    &annotationDictLen);
    if (rc)
    {
        std::cerr << "Unable to fetch annotation dictionary\n";
        return rc;
    }
    uint32_t ridSchema = resourceId >> 16 << 16;
    uint32_t schemaDictLen;
    uint8_t* schemaDict;
    rc = getDictionaryForRidDev(deviceId, ridSchema, &schemaDict,
                                    &schemaDictLen);
    if (rc)
    {
        std::cerr << "Unable to fetch schema dictionary\n";
        return rc;
    }

    std::span<uint8_t> bejson = std::span{*payload, payloadLength};
    BejDictionaries bejDictionaries;
    bejDictionaries.schemaDictionary = schemaDict;
    bejDictionaries.annotationDictionary = annotationDict;
    bejDictionaries.errorDictionary = NULL;

    if (payloadLength > 0) {
        libbej::BejDecoderJson decoder;
        decoder.decode(bejDictionaries, bejson);
        *json = decoder.getOutput();
    } else {
        *json = "{\"Status\": \"Completed\"}";
    }
    return 0;
}
/**
 * RDE Operation DBus method handler
*/
std::string
    rdeOperation(boost::asio::yield_context yield, int requestId,
                  uint8_t operationType /* Change to std::string req_type */,
                  std::string uri, std::string udevid,
                  std::string reqPayloadJson)
{
    std::cerr << "Entering DBUS Call handler with ready: " << ready << "\n";
    int rc;
    // Just using one context for now
    struct pldm_rde_requester_context baseContext;
    rc = getRdeFreeContextForRdeDevice(udevid, &baseContext);
    if (rc == RDE_NO_CONTEXT_FOUND)
    {
        std::cerr << "Unable to provide base context- Error in RDE discovery\n";
        ready = true;
        return "Unsuccessful RDE operation with error code: " +
               std::to_string(rc);
    }

    // if base context is free to execute
    if (baseContext.context_status == CONTEXT_BUSY ||
        baseContext.context_status == CONTEXT_CONTINUE)
    {
        auto timer = std::make_shared<boost::asio::steady_timer>(io);
        timer->expires_from_now(std::chrono::milliseconds(1000));
        timer->async_wait(yield);
        std::cerr << "Timer expired\n";

        int currentRetry = getCurrentRetryCount(requestId);
        if (currentRetry > MAX_RETRIES_FOR_REQUEST)
        {
            retryCounterMap.erase(requestId);
            return "Max retries exceeded for request. Abandoning...\n";
        }
        rdeOperation(yield, requestId, operationType, uri, udevid,
                      reqPayloadJson);
    }

    // get manager
    struct pldm_rde_requester_manager* manager;
    rc = getManagerForRdeDevice(udevid, &manager);
    if (rc)
    {
        std::cerr
            << "Failed to get manager to perform read request for request id: "
            << std::to_string(requestId) << ", request uri: " << uri
            << ", UDEVID: " << udevid << "\n";
        ready = true;
        return "Unsuccessful RDE operation with error code: " +
               std::to_string(rc);
    }
    uint32_t resourceId = 0;

    // strip query params - until expand is supported
    rc = removeQueryParams(&uri, "?");
    if (rc)
    {
        std::cerr << "No query params found. Using URI as is: " << uri << "\n";
    }
    else
    {
        std::cerr << "URI after removing query params: " << uri << "\n";
    }
    rc = getResourceIdForUri(uri, &resourceId);
    if (rc)
    {
        std::cerr
            << "Resource id not found, cant proceed with read operation\n";
        std::cerr << "Operation type: " << std::to_string(operationType)
                  << "\n";
        // TODO(@harshtya) - Define operation Types to be GET or POST
        if (int(operationType) !=
            8) /* check the req_type (8 is a random num denoting POST op)*/
        {
            return "Unsuccessful RDE operation with error code: " +
                   std::to_string(rc);
        }

        std::string resource_uri;
        std::string resource_name;
        std::string action;

        if (isActionOperation(uri, &resource_uri))
        {
            rc = getResourceIdForUri(resource_uri, &resourceId);
            if (rc)
            {
                std::cerr
                    << "Unable to resolve collection for RDE Action request\n";
                return "Unable to resolve collection for RDE Action request\n";
            }
            // TODO(@harshtya): Define 6 in PLDM_RDE_OPERATION_TYPES in libpldm
            operationType = 6; // This is the operation type for ACTION request
        }
        else
        {
            // Create a resource request
            std::cerr << "Operation create not supported\n";
            return "Create RDE operation not yet supported\n";
        }
    }

    uint16_t operation_id = generateOperationId();

    rc = executeRdeOperation(
        fd, DEST_EID, INSTANCE_ID, uri, udevid, operation_id, operationType,
        requestId, manager, &baseContext, resourceId, reqPayloadJson);
    if (rc)
    {
        std::cerr << "Request execution failed: ReqID: "
                  << std::to_string(requestId) << ", URI: " << uri << ", "
                  << "operation type: " << std::to_string(operationType);
        // TODO: Remove request_id from response map
        // ready = true;
        retryCounterMap.erase(requestId);
        cleanupRequestId(requestId);
        return "Unsuccessful RDE operation with error code: " +
               std::to_string(rc);
    }

    uint8_t* payload;
    uint32_t payloadLength;
    rc = getResponseForRequestId(requestId, &payload, &payloadLength);
    if (rc)
    {
        std::cerr << "Unable to fetch response from response map with rc: "
                  << rc;
        // ready = true;
        retryCounterMap.erase(requestId);
        cleanupRequestId(requestId);
        return "Unsuccessful RDE operation with error code: " +
               std::to_string(rc);
    }

    std::string jsonResp;
    rc = translateBejToString(resourceId, udevid, &jsonResp, &payload,
                                 payloadLength);
    if (rc)
    {
        std::cerr << "Unable to translate BEJ\n";
        jsonResp = "Unsuccessful RDE operation- failed to translate bej";
        cleanupRequestId(requestId);
        retryCounterMap.erase(requestId);
    }
    cleanupRequestId(requestId);
    retryCounterMap.erase(requestId);
    return jsonResp;
}
 

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

                int rc = initiateDiscovery(fd, udevid, netId, DEST_EID,
                                            INSTANCE_ID);
                if (rc)
                {
                    std::cerr
                        << "PLDM/RDE Discovery failed for device: " << port
                        << ", udev: " << udevid << "\n";
                    return;
                }

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

                iface->register_method("execute_rde", std::move(rdeOperation));

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
    rc = createResourceIdMapping();
    if (rc)
    {
        std::cerr << "Resource map creation failed\n";
        return rc;
    }
    
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
