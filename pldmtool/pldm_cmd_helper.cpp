#include "pldm_cmd_helper.hpp"

#include "libpldm/pldm.h"
#include "mctp.h"

#include "xyz/openbmc_project/Common/error.hpp"

#include <systemd/sd-bus.h>

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <exception>

using namespace pldm::utils;

namespace pldmtool
{

namespace helper
{
/*
 * Initialize the socket, send pldm command & recieve response from socket
 *
 */
int mctpSockSendRecv(const std::vector<uint8_t>& requestMsg,
                     std::vector<uint8_t>& responseMsg, bool pldmVerbose)
{
    int returnCode = 0;
    pldm_header_info responseHdrFields{};
    pldm_header_info requestHdrFields{};

    int sockFd = socket(AF_MCTP, SOCK_DGRAM, 0);
    if (-1 == sockFd)
    {
        returnCode = -errno;
        std::cerr << "Failed to create the socket : RC = " << sockFd << "\n";
        return returnCode;
    }
    Logger(pldmVerbose, "Success in creating the socket : RC = ", sockFd);

    struct sockaddr_mctp addr = {0, 0, 0, 0, 0, 0, 0};
    addr.smctp_family = AF_MCTP;
    addr.smctp_addr.s_addr = PLDM_ENTITY_ID;
    addr.smctp_type = MCTP_MSG_TYPE_PLDM;
    addr.smctp_tag = MCTP_TAG_OWNER;
    addr.smctp_network = MCTP_NET_ANY;

    CustomFD socketFd(sockFd);

    int result =
        sendto(socketFd(), requestMsg.data(), requestMsg.size(), 0,
               reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    if (-1 == result)
    {
        returnCode = -errno;
        std::cerr << "Write to socket failure : RC = " << returnCode << "\n";
        return returnCode;
    }
    Logger(pldmVerbose, "Write to socket successful : RC = ", result);
    auto hdr = reinterpret_cast<const pldm_msg_hdr*>(requestMsg.data());

    if (PLDM_SUCCESS != unpack_pldm_header(hdr, &requestHdrFields))
    {
        std::cerr << "Empty PLDM request header \n";
    }

    /* We need to make sure the buffer can hold the whole message or we could
     * lose bits */
    /* TODO? Add timeout here? */
    socklen_t addrlen = sizeof(struct sockaddr_mctp);
    ssize_t peekedLength =
        recvfrom(socketFd(), responseMsg.data(), responseMsg.size(),
                 MSG_PEEK | MSG_TRUNC,
                 reinterpret_cast<struct sockaddr*>(&addr), &addrlen);

    responseMsg.resize(peekedLength);
    do
    {
        auto recvDataLength =
            recv(socketFd(), reinterpret_cast<void*>(responseMsg.data()),
                 peekedLength, 0);
        hdr = reinterpret_cast<const pldm_msg_hdr*>(responseMsg.data());
        if (PLDM_SUCCESS != unpack_pldm_header(hdr, &responseHdrFields))
        {
            std::cerr << "Empty PLDM response header \n";
        }

        if (recvDataLength == peekedLength &&
            responseHdrFields.instance == requestHdrFields.instance &&
            responseHdrFields.msg_type != PLDM_REQUEST)
        {
            Logger(pldmVerbose, "Total length:", recvDataLength);
            break;
        }
        else if (recvDataLength != peekedLength)
        {
            std::cerr << "Failure to read response length packet: length = "
                      << recvDataLength << "and result " << result << "\n";
            return returnCode;
        }
    } while (1);

    return PLDM_SUCCESS;
}

void CommandInterface::exec()
{
    static constexpr auto pldmObjPath = "/xyz/openbmc_project/pldm";
    static constexpr auto pldmRequester = "xyz.openbmc_project.PLDM.Requester";
    auto& bus = pldm::utils::DBusHandler::getBus();
    try
    {
        auto service =
            pldm::utils::DBusHandler().getService(pldmObjPath, pldmRequester);
        auto method = bus.new_method_call(service.c_str(), pldmObjPath,
                                          pldmRequester, "GetInstanceId");
        method.append(mctp_eid);
        auto reply = bus.call(method);
        reply.read(instanceId);
    }
    catch (const std::exception& e)
    {
        std::cerr << "GetInstanceId D-Bus call failed, MCTP id = "
                  << (unsigned)mctp_eid << ", error = " << e.what() << "\n";
        return;
    }
    auto [rc, requestMsg] = createRequestMsg();
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Failed to encode request message for " << pldmType << ":"
                  << commandName << " rc = " << rc << "\n";
        return;
    }

    std::vector<uint8_t> responseMsg;
    rc = pldmSendRecv(requestMsg, responseMsg);

    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "pldmSendRecv: Failed to receive RC = " << rc << "\n";
        return;
    }

    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg.data());
    parseResponseMsg(responsePtr, responseMsg.size() - sizeof(pldm_msg_hdr));
}

int CommandInterface::pldmSendRecv(std::vector<uint8_t>& requestMsg,
                                   std::vector<uint8_t>& responseMsg)
{
    bool mctpVerbose = pldmVerbose;

    // By default enable request/response msgs for pldmtool raw commands.
    if (CommandInterface::pldmType == "raw")
    {
        pldmVerbose = true;
    }

    if (pldmVerbose)
    {
        std::cout << "pldmtool: ";
        printBuffer(Tx, requestMsg);
    }

    if (mctp_eid != PLDM_ENTITY_ID)
    {
        int fd = pldm_open();
        if (-1 == fd)
        {
            std::cerr << "failed to init mctp "
                      << "\n";
            return -1;
        }
        uint8_t* responseMessage = nullptr;
        size_t responseMessageSize{};
        pldm_send_recv(mctp_eid, fd, requestMsg.data() + 2,
                       requestMsg.size() - 2, &responseMessage,
                       &responseMessageSize);

        responseMsg.resize(responseMessageSize);
        memcpy(responseMsg.data(), responseMessage, responseMsg.size());

        free(responseMessage);
        if (pldmVerbose)
        {
            std::cout << "pldmtool: ";
            printBuffer(Rx, responseMsg);
        }
    }
    else
    {
        mctpSockSendRecv(requestMsg, responseMsg, mctpVerbose);
        if (pldmVerbose)
        {
            std::cout << "pldmtool: ";
            printBuffer(Rx, responseMsg);
        }
    }
    return PLDM_SUCCESS;
}
} // namespace helper
} // namespace pldmtool
