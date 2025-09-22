#include "pldm_cmd_helper.hpp"

#include "common/transport.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <libpldm/firmware_update.h>
#include <libpldm/transport.h>
#include <libpldm/transport/af-mctp.h>
#include <libpldm/transport/mctp-demux.h>
#include <linux/mctp.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <systemd/sd-bus.h>

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <cstring>
#include <exception>

using namespace pldm::utils;

namespace pldmtool
{
namespace helper
{

static const std::map<uint8_t, std::string> genericCompletionCodes{
    {PLDM_SUCCESS, "SUCCESS"},
    {PLDM_ERROR, "ERROR"},
    {PLDM_ERROR_INVALID_DATA, "ERROR_INVALID_DATA"},
    {PLDM_ERROR_INVALID_LENGTH, "ERROR_INVALID_LENGTH"},
    {PLDM_ERROR_NOT_READY, "ERROR_NOT_READY"},
    {PLDM_ERROR_UNSUPPORTED_PLDM_CMD, "ERROR_UNSUPPORTED_PLDM_CMD"},
    {PLDM_ERROR_INVALID_PLDM_TYPE, "ERROR_INVALID_PLDM_TYPE"},
    {PLDM_INVALID_TRANSFER_OPERATION_FLAG, "INVALID_TRANSFER_OPERATION_FLAG"}};

static const std::map<uint8_t, std::string> fwupdateCompletionCodes{
    {PLDM_FWUP_NOT_IN_UPDATE_MODE, "NOT_IN_UPDATE_MODE"},
    {PLDM_FWUP_ALREADY_IN_UPDATE_MODE, "ALREADY_IN_UPDATE_MODE"},
    {PLDM_FWUP_DATA_OUT_OF_RANGE, "DATA_OUT_OF_RANGE"},
    {PLDM_FWUP_INVALID_TRANSFER_LENGTH, "INVALID_TRANSFER_LENGTH"},
    {PLDM_FWUP_INVALID_STATE_FOR_COMMAND, "INVALID_STATE_FOR_COMMAND"},
    {PLDM_FWUP_INCOMPLETE_UPDATE, "INCOMPLETE_UPDATE"},
    {PLDM_FWUP_BUSY_IN_BACKGROUND, "BUSY_IN_BACKGROUND"},
    {PLDM_FWUP_CANCEL_PENDING, "CANCEL_PENDING"},
    {PLDM_FWUP_COMMAND_NOT_EXPECTED, "COMMAND_NOT_EXPECTED"},
    {PLDM_FWUP_RETRY_REQUEST_FW_DATA, "RETRY_REQUEST_FW_DATA"},
    {PLDM_FWUP_UNABLE_TO_INITIATE_UPDATE, "UNABLE_TO_INITIATE_UPDATE"},
    {PLDM_FWUP_ACTIVATION_NOT_REQUIRED, "ACTIVATION_NOT_REQUIRED"},
    {PLDM_FWUP_SELF_CONTAINED_ACTIVATION_NOT_PERMITTED,
     "SELF_CONTAINED_ACTIVATION_NOT_PERMITTED"},
    {PLDM_FWUP_NO_DEVICE_METADATA, "NO_DEVICE_METADATA"},
    {PLDM_FWUP_RETRY_REQUEST_UPDATE, "RETRY_REQUEST_UPDATE"},
    {PLDM_FWUP_NO_PACKAGE_DATA, "NO_PACKAGE_DATA"},
    {PLDM_FWUP_INVALID_TRANSFER_HANDLE, "INVALID_TRANSFER_HANDLE"},
    {PLDM_FWUP_INVALID_TRANSFER_OPERATION_FLAG,
     "INVALID_TRANSFER_OPERATION_FLAG"},
    {PLDM_FWUP_ACTIVATE_PENDING_IMAGE_NOT_PERMITTED,
     "ACTIVATE_PENDING_IMAGE_NOT_PERMITTED"},
    {PLDM_FWUP_PACKAGE_DATA_ERROR, "PACKAGE_DATA_ERROR"}};

void fillCompletionCode(uint8_t completionCode, ordered_json& data,
                        uint8_t pldmType)
{
    // Check generic completion codes first for all PLDM types
    auto it = genericCompletionCodes.find(completionCode);
    if (it != genericCompletionCodes.end())
    {
        data["CompletionCode"] = it->second;
        return;
    }

    // If not a generic code, check type-specific codes
    switch (pldmType)
    {
        case PLDM_FWUP:
        {
            auto typeIt = fwupdateCompletionCodes.find(completionCode);
            if (typeIt != fwupdateCompletionCodes.end())
            {
                data["CompletionCode"] = typeIt->second;
                return;
            }
            break;
        }
    }

    data["CompletionCode"] = "UNKNOWN_COMPLETION_CODE";
}

int mctpSockSendRecv(const uint8_t mctpNetworkId, const uint8_t eid,
                     const bool mctpPreAllocTag,
                     const std::vector<uint8_t>& requestMsg,
                     void** responseMessage, size_t* responseMessageSize)
{
    struct sockaddr_mctp_ext addr;
    int sd;
    int rc;
    struct mctp_ioc_tag_ctl ctl = {
        .peer_addr = eid,
        .tag = 0,
        .flags = 0,
    };

    // open AF_MCTP socket
    sd = socket(AF_MCTP, SOCK_DGRAM, 0);
    if (sd < 0)
    {
        rc = -errno;
        std::cerr << "socket(AF_MCTP, SOCK_DGRAM, 0) failed. errnostr = "
                  << strerror(errno) << "\n";
        return rc;
    }

    // We want extended addressing on all received messages
    int val = 1;
    rc = setsockopt(sd, SOL_MCTP, MCTP_OPT_ADDR_EXT, &val, sizeof(val));
    if (rc < 0)
    {
        rc = -errno;
        std::cerr
            << "Kernel does not support MCTP extended addressing. errnostr = "
            << strerror(errno) << "\n";
        close(sd);
        return rc;
    }

    // prepare the request to be sent
    memset(&addr, 0, sizeof(addr));
    addr.smctp_base.smctp_family = AF_MCTP;
    addr.smctp_base.smctp_network = mctpNetworkId;
    addr.smctp_base.smctp_addr.s_addr = eid;
    addr.smctp_base.smctp_type = requestMsg[0];
    if (mctpPreAllocTag)
    {
        // preallocate a tag if needed
        rc = ioctl(sd, SIOCMCTPALLOCTAG, &ctl);
        if (rc)
        {
            rc = -errno;
            std::cerr << "ioctl(SIOCMCTPALLOCTAG) failed. errnostr = "
                      << strerror(errno) << "\n";
            close(sd);
            return rc;
        }
    } // preAllocTag

    // send the MCTP message
    addr.smctp_base.smctp_tag = MCTP_TAG_OWNER | ctl.tag;
    rc = sendto(sd, &requestMsg[1], requestMsg.size() - 1, 0,
                (struct sockaddr*)&addr, sizeof(struct sockaddr_mctp));
    if (rc < 0)
    {
        rc = -errno;
        std::cerr << "sendto(AF_MCTP) failed. errnostr = " << strerror(errno)
                  << "\n";
        close(sd);
        return rc;
    }

    // wait for for the response from the MCTP Endpoint
    // Instance ID expiration interval (MT4) - after which the instance ID
    // will be reused. For PCIe binding this timeout is 5 seconds.
    const int MCTP_INST_ID_EXPIRATION_INTERVAL_MT4 = 5;
    struct pollfd pollfd;
    pollfd.fd = sd;
    pollfd.events = POLLIN;
    rc = poll(&pollfd, 1, MCTP_INST_ID_EXPIRATION_INTERVAL_MT4 * 1000);
    if (rc < 0)
    {
        std::cerr << "poll(AF_MCTP, 5000) failed. errnostr = "
                  << strerror(errno) << "\n";
        close(sd);
        return rc;
    }
    else if (rc == 0)
    {
        // poll() timed out
        std::cerr << "Timeout(5s): No response from the endpoint\n";
        close(sd);
        return rc;
    }
    else
    {
        // data on the socket
        // take a PEEK at the socket to know how many bytes to read
        int respLen = recvfrom(sd, NULL, 0, MSG_PEEK | MSG_TRUNC, NULL, 0);
        if (respLen < 0)
        {
            rc = -errno;
            std::cerr << "recvfrom(MSG_PEEK | MSG_TRUNC)failed. errnostr = "
                      << strerror(errno) << "\n";
            close(sd);
            return rc;
        }

        // read the received data
        struct sockaddr_mctp retAddr;
        socklen_t addrlen = sizeof(retAddr);
        memset(&retAddr, 0x0, sizeof(retAddr));
        uint8_t* respBuf;
        respBuf = (uint8_t*)malloc(respLen);
        int rcvdByteCount = recvfrom(sd, respBuf, respLen, MSG_TRUNC,
                                     (struct sockaddr*)&retAddr, &addrlen);
        if (rcvdByteCount < 0)
        {
            rc = -errno;
            std::cerr << "recvfrom(): DATA failed: " << strerror(errno) << "\n";
            free(respBuf);
            close(sd);
            return rc;
        }
        if (mctpPreAllocTag)
        {
            // drop the preallocated msg tag
            rc = ioctl(sd, SIOCMCTPDROPTAG, &ctl);
            if (rc)
            {
                rc = -errno;
                std::cerr << "ioctl(SIOCMCTPDROPTAG) failed. errnostr = "
                          << strerror(errno) << "\n";
                free(respBuf);
                close(sd);
                return rc;
            }
        }
        *responseMessageSize = rcvdByteCount;
        *responseMessage = (void*)respBuf;
    }

    close(sd);
    return 0;
}

void CommandInterface::exec()
{
    instanceId = instanceIdDb.next(mctp_eid);
    auto [rc, requestMsg] = createRequestMsg();
    if (rc != PLDM_SUCCESS)
    {
        instanceIdDb.free(mctp_eid, instanceId);
        std::cerr << "Failed to encode request message for " << pldmType << ":"
                  << commandName << " rc = " << rc << "\n";
        return;
    }

    std::vector<uint8_t> responseMsg;
    rc = pldmSendRecv(requestMsg, responseMsg);

    if (rc != PLDM_SUCCESS)
    {
        instanceIdDb.free(mctp_eid, instanceId);
        std::cerr << "pldmSendRecv: Failed to receive RC = " << rc << "\n";
        return;
    }

    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg.data());
    parseResponseMsg(responsePtr, responseMsg.size() - sizeof(pldm_msg_hdr));
    instanceIdDb.free(mctp_eid, instanceId);
}

std::set<pldm::dbus::Service> CommandInterface::getMctpServices() const
{
    pldm::utils::GetSubTreeResponse getSubTreeResponse{};
    std::set<pldm::dbus::Service> mctpCtrlServices{};
    const pldm::dbus::Interfaces ifaceList{mctpEndpointIntfName};
    try
    {
        getSubTreeResponse = pldm::utils::DBusHandler().getSubtree(
            "/xyz/openbmc_project/mctp", 0, ifaceList);
    }
    catch (const std::exception& e)
    {
        std::cerr << "D-Bus error calling Subtrees method on ObjectMapper: "
                  << e.what() << '\n';
    }

    for (const auto& [objPath, mapperServiceMap] : getSubTreeResponse)
    {
        for (const auto& [service, interfaces] : mapperServiceMap)
        {
            mctpCtrlServices.insert(service);
        }
    }

    return mctpCtrlServices;
}

pldm::dbus::ObjectValueTree CommandInterface::getMctpManagedObjects(
    const std::string& service) const noexcept
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    pldm::dbus::ObjectValueTree objects{};
    try
    {
        pldm::dbus::ObjectValueTree tmpObjects{};
        auto method = bus.new_method_call(
            service.c_str(), "/xyz/openbmc_project/mctp",
            "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
        auto reply = bus.call(method);
        reply.read(tmpObjects);
        objects.insert(tmpObjects.begin(), tmpObjects.end());
    }
    catch (const std::exception& e)
    {
        std::cerr << "D-Bus error while fetching MCTP managed objects for: "
                  << service << '\n'
                  << e.what() << '\n';
    }
    return objects;
}

std::tuple<bool, int, int, std::vector<uint8_t>>
    CommandInterface::getMctpSockInfo(uint8_t remoteEID)
{
    using namespace pldm;
    int type = 0;
    int protocol = 0;
    bool enabled = false;
    std::vector<uint8_t> address{};

    const auto& mctpCtrlServices = getMctpServices();

    for (const auto& serviceName : mctpCtrlServices)
    {
        const auto& objects = getMctpManagedObjects(serviceName);

        for (const auto& [objectPath, interfaces] : objects)
        {
            if (interfaces.contains(mctpEndpointIntfName))
            {
                const auto& mctpProperties =
                    interfaces.at(mctpEndpointIntfName);
                auto eid = std::get<size_t>(mctpProperties.at("EID"));
                if (remoteEID != eid)
                {
                    continue;
                }
                if (!interfaces.contains(unixSocketIntfName))
                {
                    continue;
                }

                const auto& properties = interfaces.at(unixSocketIntfName);
                type = std::get<size_t>(properties.at("Type"));
                protocol = std::get<size_t>(properties.at("Protocol"));
                address =
                    std::get<std::vector<uint8_t>>(properties.at("Address"));

                if (interfaces.contains(objectEnableIntfName))
                {
                    const auto& propertiesEnable =
                        interfaces.at(objectEnableIntfName);
                    enabled = std::get<bool>(propertiesEnable.at("Enabled"));
                }

                if (address.empty() || !type)
                {
                    address.clear();
                    return {false, 0, 0, address};
                }
                else
                {
                    return {enabled, type, protocol, address};
                }
            }
        }
    }

    return {enabled, type, protocol, address};
}

int CommandInterface::pldmSendRecv(std::vector<uint8_t>& requestMsg,
                                   std::vector<uint8_t>& responseMsg)
{
    requestMsg.insert(requestMsg.begin(), MCTP_MSG_TYPE_PLDM);
    requestMsg.insert(requestMsg.begin(), mctp_eid);
    // By default enable request/response msgs for pldmtool raw commands.
    if ((CommandInterface::pldmType == "raw") ||
        (CommandInterface::pldmType == "mctpRaw"))
    {
        pldmVerbose = true;
    }

    if (pldmVerbose)
    {
        std::cout << "pldmtool: ";
        printBuffer(Tx, requestMsg);
    }

    /*
    auto tid = mctp_eid;
    PldmTransport pldmTransport{};
    uint8_t retry = 0;
    int rc = PLDM_ERROR;
    */

    if (mctp_eid != PLDM_ENTITY_ID)
    {
        auto [enabled, type, protocol, sockAddress] = getMctpSockInfo(mctp_eid);
        if (sockAddress.empty())
        {
            std::cerr << "pldmtool: Remote MCTP endpoint not found"
                      << "\n";
            return -1;
        }

        if (!enabled)
        {
            std::cerr << "pldmtool: Remote MCTP endpoint is disabled"
                      << "\n";
            return -1;
        }

        int rc = 0;
        int sockFd = socket(AF_UNIX, type, protocol);
        if (-1 == sockFd)
        {
            rc = -errno;
            std::cerr << "Failed to create the socket : RC = " << sockFd
                      << "\n";
            return rc;
        }
        Logger(pldmVerbose, "Success in creating the socket : RC = ", sockFd);

        CustomFD socketFd(sockFd);

        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        memcpy(addr.sun_path, sockAddress.data(), sockAddress.size());
        rc = connect(sockFd, reinterpret_cast<struct sockaddr*>(&addr),
                     sockAddress.size() + sizeof(addr.sun_family));
        if (-1 == rc)
        {
            rc = -errno;
            std::cerr << "Failed to connect to socket : RC = " << rc << "\n";
            return rc;
        }
        Logger(pldmVerbose, "Success in connecting to socket : RC = ", rc);

        auto pldmType = MCTP_MSG_TYPE_PLDM;
        rc = write(socketFd(), &pldmType, sizeof(pldmType));
        if (-1 == rc)
        {
            rc = -errno;
            std::cerr
                << "Failed to send message type as pldm to mctp demux daemon: RC = "
                << rc << "\n";
            return rc;
        }
        Logger(
            pldmVerbose,
            "Success in sending message type as pldm to mctp demux daemon : RC = ",
            rc);

        uint8_t* responseMessage = nullptr;
        size_t responseMessageSize{};
        rc = pldm_send_recv(mctp_eid, sockFd, requestMsg.data() + 2,
                       requestMsg.size() - 2, &responseMessage,
                       &responseMessageSize);
        responseMsg.resize(responseMessageSize);
        memcpy(responseMsg.data(), responseMessage, responseMsg.size());

        free(responseMessage);
        if (pldmVerbose)
        {
            std::cerr << "pldmtool: ";
            printBuffer(Rx, responseMsg);
        }
    }
    /*
    while (PLDM_REQUESTER_SUCCESS != rc && retry <= numRetries)
    {
        void* responseMessage = nullptr;
        size_t responseMessageSize{};

        if (CommandInterface::pldmType != "mctpRaw")
        {
            rc = pldmTransport.sendRecvMsg(tid, requestMsg.data(),
                                           requestMsg.size(), responseMessage,
                                           responseMessageSize);
            if (rc)
            {
                std::cerr << "[" << unsigned(retry)
                          << "] pldm_send_recv error rc " << rc << std::endl;
                retry++;
                continue;
            }
        }
        else
        {
            rc = mctpSockSendRecv(mctpNetworkId, mctp_eid, mctpPreAllocTag,
                                  requestMsg, &responseMessage,
                                  &responseMessageSize);
            if (rc)
            {
                std::cerr << "[" << unsigned(retry)
                          << "] mctpSockSendRecv() error rc " << rc
                          << std::endl;
                retry++;
                continue;
            }
        }

        responseMsg.resize(responseMessageSize);
        memcpy(responseMsg.data(), responseMessage, responseMsg.size());

        free(responseMessage);

        if (pldmVerbose)
        {
            std::cout << "pldmtool: ";
            printBuffer(Rx, responseMsg);
        }
    }

    if (rc)
    {
        std::cerr << "failed to pldm send recv error rc " << rc << std::endl;
    }
    */
    return PLDM_SUCCESS;
}
} // namespace helper
} // namespace pldmtool
