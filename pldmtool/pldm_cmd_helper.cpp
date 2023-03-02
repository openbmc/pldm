#include "pldm_cmd_helper.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <libpldm/pldm.h>
#include <systemd/sd-bus.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <exception>

PHOSPHOR_LOG2_USING;

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
    const char devPath[] = "\0mctp-mux";
    int returnCode = 0;

    int sockFd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (-1 == sockFd)
    {
        returnCode = -errno;
        error("Failed to create the socket : RC = {RC}", "RC", sockFd);
        return returnCode;
    }
    Logger(pldmVerbose, "Success in creating the socket : RC = ", sockFd);

    struct sockaddr_un addr
    {};
    addr.sun_family = AF_UNIX;

    memcpy(addr.sun_path, devPath, sizeof(devPath) - 1);

    CustomFD socketFd(sockFd);
    int result = connect(socketFd(), reinterpret_cast<struct sockaddr*>(&addr),
                         sizeof(devPath) + sizeof(addr.sun_family) - 1);
    if (-1 == result)
    {
        returnCode = -errno;
        error("Failed to connect to socket : RC = {RC}", "RC", returnCode);

        return returnCode;
    }
    Logger(pldmVerbose, "Success in connecting to socket : RC = ", returnCode);

    auto pldmType = MCTP_MSG_TYPE_PLDM;
    result = write(socketFd(), &pldmType, sizeof(pldmType));
    if (-1 == result)
    {
        returnCode = -errno;
        error("Failed to send message type as pldm to mctp : RC = {RC}", "RC",
              returnCode);
        return returnCode;
    }
    Logger(
        pldmVerbose,
        "Success in sending message type as pldm to mctp : RC = ", returnCode);

    result = send(socketFd(), requestMsg.data(), requestMsg.size(), 0);
    if (-1 == result)
    {
        returnCode = -errno;
        error("Write to socket failure : RC = {RC}", "RC", returnCode);
        return returnCode;
    }
    Logger(pldmVerbose, "Write to socket successful : RC = ", result);

    // Read the response from socket
    ssize_t peekedLength = recv(socketFd(), nullptr, 0, MSG_TRUNC | MSG_PEEK);
    if (0 == peekedLength)
    {
        error("Socket is closed : peekedLength = {LEN}", "LEN", peekedLength);

        return returnCode;
    }
    else if (peekedLength <= -1)
    {
        returnCode = -errno;
        error("recv() system call failed : RC = {RC}", "RC", returnCode);
        return returnCode;
    }
    else
    {
        auto reqhdr = reinterpret_cast<const pldm_msg_hdr*>(&requestMsg[2]);
        do
        {
            ssize_t peekedLength =
                recv(socketFd(), nullptr, 0, MSG_PEEK | MSG_TRUNC);
            responseMsg.resize(peekedLength);
            auto recvDataLength =
                recv(socketFd(), reinterpret_cast<void*>(responseMsg.data()),
                     peekedLength, 0);
            auto resphdr =
                reinterpret_cast<const pldm_msg_hdr*>(&responseMsg[2]);
            if (recvDataLength == peekedLength &&
                resphdr->instance_id == reqhdr->instance_id &&
                resphdr->request != PLDM_REQUEST)
            {
                Logger(pldmVerbose, "Total length:", recvDataLength);
                break;
            }
            else if (recvDataLength != peekedLength)
            {
                error(
                    "Failure to read response length packet: length = {RECV_LEN}",
                    "RECV_LEN", recvDataLength);
                return returnCode;
            }
        } while (1);
    }

    returnCode = shutdown(socketFd(), SHUT_RDWR);
    if (-1 == returnCode)
    {
        returnCode = -errno;
        error("Failed to shutdown the socket : RC ={RC}", "RC", returnCode);
        return returnCode;
    }

    Logger(pldmVerbose, "Shutdown Socket successful :  RC = ", returnCode);
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
        error(
            "GetInstanceId D-Bus call failed, MCTP id = {MCTP_EID}, error = {ERR_EXCEP}",
            "MCTP_EID", (unsigned)mctp_eid, "ERR_EXCEP", e.what());
        return;
    }
    auto [rc, requestMsg] = createRequestMsg();
    if (rc != PLDM_SUCCESS)
    {
        error(
            "Failed to encode request message for {PLDM_TYPE} : {CMD_NAME}, rc = {RC}",
            "PLDM_TYPE", pldmType, "CMD_NAME", commandName, "RC", rc);
        return;
    }

    std::vector<uint8_t> responseMsg;
    rc = pldmSendRecv(requestMsg, responseMsg);

    if (rc != PLDM_SUCCESS)
    {
        error("pldmSendRecv: Failed to receive RC = {RC}", "RC", rc);
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
            error("failed to init mctp ");

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
        responseMsg.erase(responseMsg.begin(),
                          responseMsg.begin() + 2 /* skip the mctp header */);
    }
    return PLDM_SUCCESS;
}
} // namespace helper
} // namespace pldmtool
