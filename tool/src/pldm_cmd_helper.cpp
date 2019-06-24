#include "inc/pldm_cmd_helper.hpp"

constexpr uint8_t MCTP_MSG_TYPE_PLDM = 1;
constexpr uint8_t PLDM_ENTITY_ID = 8;

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
    Logger::Info(tempStream.str().c_str());
}

/*
 * Initialize the socket to send & recieve response from socket
 *
 */
int mctp_sock_init()
{
    const char devPath[] = "\0mctp-mux";
    struct sockaddr_un addr
    {
    };
    addr.sun_family = AF_UNIX;
    int returnCode = 0;

    int socketFd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (-1 == socketFd)
    {
        returnCode = -errno;
        LOG_E("Failed to create the socket : RC = [%d]", returnCode);
        return returnCode;
    }
    LOG_D("Success in creating the socket : RC = [%d]", returnCode);

    memcpy(addr.sun_path, devPath, sizeof(devPath) - 1);

    int result = connect(socketFd, reinterpret_cast<struct sockaddr*>(&addr),
                     sizeof(devPath) + sizeof(addr.sun_family) - 1);
    if (-1 == result)
    {
        returnCode = -errno;
        LOG_E("Failed to connect to the socket : RC = [%d]", returnCode);
        close(socketFd);
        return returnCode;
    }
    LOG_D("Success in connecting to the socket : RC = [%d]", returnCode);

    auto pldmType = MCTP_MSG_TYPE_PLDM;
    result = write(socketFd, &pldmType, sizeof(pldmType));
    if (-1 == result)
    {
        returnCode = -errno;
        LOG_E("Failed to send message type as pldm to mctp : RC = [%d]",
              returnCode);
        close(socketFd);
        return returnCode;
    }
    LOG_D("Success in sending message type as pldm to mctp : RC = [%d]",
          returnCode);

    return socketFd;
}

/*
 * Generic API to read/recieve the data from socket
 *
 */
int mctp_sock_recv(int socketFd, std::vector<uint8_t> requestMsg,
                   std::vector<uint8_t>& responseMsg)
{
    int returnCode = -1;

    // Read the response from socket
    ssize_t peekedLength = recv(socketFd, nullptr, 0, MSG_TRUNC | MSG_PEEK);
    if (0 == peekedLength)
    {
        LOG_E("Socket has been closed : peekedLength = [%d]", peekedLength);
        close(socketFd);
        return returnCode;
    }
    else if (peekedLength <= -1)
    {
        returnCode = -errno;
        LOG_E("recv() system call failed, RC = [%d]", returnCode);
        close(socketFd);
        return returnCode;
    }
    else
    {
        // loopback response message
        std::vector<uint8_t> lbrespMsg(peekedLength);
        auto recvDataLength =
            recv(socketFd, reinterpret_cast<void*>(lbrespMsg.data()),
                 peekedLength, 0);
        if (recvDataLength == peekedLength)
        {
            LOG_I("Total length [%zu]\n"
                  "Loopback response message:",
                  recvDataLength);
            printBuffer(lbrespMsg);
        }
        else
        {
            LOG_E("Failure to read peeked length packet", returnCode);
            close(socketFd);
            return returnCode;
        }

        // Comparing if the data recieved on first attempt Vs. request sent
        returnCode =
            std::memcmp(requestMsg.data(), lbrespMsg.data(), requestMsg.size());

        // If response = request in first recv() attempt then, read again
        // to get actual command response
        auto respEid = lbrespMsg.data()[0];
        auto respType = lbrespMsg.data()[1];
        LOG_I("Entity ID = %d", respEid);
        LOG_I("PLDM Type = %d", respType);
        if ((!returnCode) &&
            (respEid == PLDM_ENTITY_ID && respType == MCTP_MSG_TYPE_PLDM))
        {

            LOG_D("On first recv(), responseMsg == requestMsg", returnCode);
            ssize_t peekedLength =
                recv(socketFd, nullptr, 0, MSG_PEEK | MSG_TRUNC);

            responseMsg.resize(peekedLength);
            recvDataLength =
                recv(socketFd, reinterpret_cast<void*>(responseMsg.data()),
                     peekedLength, 0);
            if (recvDataLength == peekedLength)
            {
                LOG_I("Total length [%zu]", recvDataLength);
                return PLDM_SUCCESS;
            }
            else
            {
                LOG_E("Failure to read response length packet", recvDataLength);
                close(socketFd);
                return returnCode;
            }
        }
        else
        {
            LOG_E("On first recv(), requestMsg != responseMsg", returnCode);
            close(socketFd);
            return returnCode;
        }
    }
}
