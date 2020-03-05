#include "pldm_cmd_helper.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <systemd/sd-bus.h>

#include <exception>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include "libpldm/requester/pldm.h"

namespace pldmtool
{

namespace helper
{

uint8_t PLDM_LOCAL_INSTANCE_ID = 0;
/*
 * print the input buffer
 *
 */
void printBuffer(const std::vector<uint8_t>& buffer)
{
    std::ostringstream tempStream;
    if (!buffer.empty())
    {
        for (int byte : buffer)
        {
            tempStream << std::setfill('0') << std::setw(2) << std::hex << byte
                       << " ";
        }
    }
    std::cout << tempStream.str() << std::endl;
}
/*
 * Initialize the socket, send pldm command & recieve response from socket
 *
 */
int mctpSockSendRecv(const std::vector<uint8_t>& requestMsg,
                     std::vector<uint8_t>& responseMsg)
{
    const char devPath[] = "\0mctp-mux";
    int returnCode = 0;

    int sockFd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (-1 == sockFd)
    {
        returnCode = -errno;
        std::cerr << "Failed to create the socket : RC = " << sockFd << "\n";
        return returnCode;
    }
    std::cout << "Success in creating the socket : RC = " << sockFd
              << std::endl;

    struct sockaddr_un addr
    {
    };
    addr.sun_family = AF_UNIX;

    memcpy(addr.sun_path, devPath, sizeof(devPath) - 1);

    CustomFD socketFd(sockFd);
    int result = connect(socketFd(), reinterpret_cast<struct sockaddr*>(&addr),
                         sizeof(devPath) + sizeof(addr.sun_family) - 1);
    if (-1 == result)
    {
        returnCode = -errno;
        std::cerr << "Failed to connect to socket : RC = " << returnCode
                  << "\n";
        return returnCode;
    }
    std::cout << "Success in connecting to socket : RC = " << returnCode
              << std::endl;

    auto pldmType = MCTP_MSG_TYPE_PLDM;
    result = write(socketFd(), &pldmType, sizeof(pldmType));
    if (-1 == result)
    {
        returnCode = -errno;
        std::cerr << "Failed to send message type as pldm to mctp : RC = "
                  << returnCode << "\n";
        return returnCode;
    }
    std::cout << "Success in sending message type as pldm to mctp : RC = "
              << returnCode << std::endl;

    result = send(socketFd(), requestMsg.data(), requestMsg.size(), 0);
    if (-1 == result)
    {
        returnCode = -errno;
        std::cerr << "Write to socket failure : RC = " << returnCode << "\n";
        return returnCode;
    }
    std::cout << "Write to socket successful : RC = " << result << std::endl;

    // Read the response from socket
    ssize_t peekedLength = recv(socketFd(), nullptr, 0, MSG_TRUNC | MSG_PEEK);
    if (0 == peekedLength)
    {
        std::cerr << "Socket is closed : peekedLength = " << peekedLength
                  << "\n";
        return returnCode;
    }
    else if (peekedLength <= -1)
    {
        returnCode = -errno;
        std::cerr << "recv() system call failed : RC = " << returnCode << "\n";
        return returnCode;
    }
    else
    {
        // Confirming on the first recv() the Request bit is set in
        // pldm_msg_hdr struct. If set proceed with recv() or else, quit.
        auto hdr = reinterpret_cast<const pldm_msg_hdr*>(&requestMsg[2]);
        uint8_t request = hdr->request;
        if (request == PLDM_REQUEST)
        {
            std::cout << "On first recv(),response == request : RC = "
                      << returnCode << std::endl;
            do
            {
                ssize_t peekedLength =
                    recv(socketFd(), nullptr, 0, MSG_PEEK | MSG_TRUNC);
                responseMsg.resize(peekedLength);
                auto recvDataLength = recv(
                    socketFd(), reinterpret_cast<void*>(responseMsg.data()),
                    peekedLength, 0);
                if (recvDataLength == peekedLength &&
                    responseMsg[2] == PLDM_LOCAL_INSTANCE_ID)
                {
                    std::cout << "Total length: " << recvDataLength
                              << std::endl;
                    break;
                }
            } while (1);
        }
        else
        {
            std::cerr << "On first recv(),request != response : RC = "
                      << returnCode << "\n";
            return returnCode;
        }
    }

    returnCode = shutdown(socketFd(), SHUT_RDWR);
    if (-1 == returnCode)
    {
        returnCode = -errno;
        std::cerr << "Failed to shutdown the socket : RC = " << returnCode
                  << "\n";
        return returnCode;
    }

    std::cout << "Shutdown Socket successful :  RC = " << returnCode
              << std::endl;
    return PLDM_SUCCESS;
}

void CommandInterface::exec()
{
    static constexpr auto pldmObjPath = "/xyz/openbmc_project/pldm";
    static constexpr auto pldmInterface = "xyz.openbmc_project.PLDM.Requester";
    auto& bus = pldm::utils::DBusHandler::getBus();
    try
    {
        auto service =
            pldm::utils::DBusHandler().getService(pldmObjPath, pldmInterface);
        auto method = bus.new_method_call(service.c_str(), pldmObjPath,
                                          pldmInterface, "GetInstanceId");
        method.append(mctp_eid);
        auto reply = bus.call(method);
        reply.read(instanceId);
        PLDM_LOCAL_INSTANCE_ID = instanceId;
    }
    catch (const std::exception& e)
    {
        std::cerr << "GetInstanceId D-Bus call failed, MCTP id = " << mctp_eid
                  << ", error = " << e.what() << "\n";
        return;
    }
    auto [rc, requestMsg] = createRequestMsg();
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Failed to encode request message for " << pldmType << ":"
                  << commandName << " rc = " << rc << "\n";
        return;
    }

    std::cout << "Encode request successfully" << std::endl;

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
    // Insert the PLDM message type and EID at the beginning of the
    // msg.
    requestMsg.insert(requestMsg.begin(), MCTP_MSG_TYPE_PLDM);
    requestMsg.insert(requestMsg.begin(), mctp_eid);

    std::cout << "Request Message:" << std::endl;
    printBuffer(requestMsg);

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
        std::cout << "Response Message:" << std::endl;

        responseMsg.resize(responseMessageSize);
        memcpy(responseMsg.data(), responseMessage, responseMsg.size());

        free(responseMessage);
        printBuffer(responseMsg);
    }
    else
    {
        mctpSockSendRecv(requestMsg, responseMsg);
        std::cout << "Response Message:" << std::endl;
        printBuffer(responseMsg);
        responseMsg.erase(responseMsg.begin(),
                          responseMsg.begin() + 2 /* skip the mctp header */);
    }
    return PLDM_SUCCESS;
}
} // namespace helper
} // namespace pldmtool
