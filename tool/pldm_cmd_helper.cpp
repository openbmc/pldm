#include "pldm_cmd_helper.hpp"

constexpr uint8_t MCTP_MSG_TYPE_PLDM = 1;
constexpr uint8_t PLDM_ENTITY_ID = 8;

using namespace std;

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
    cout << tempStream.str().c_str() << endl;
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
        cerr << "Failed to create the socket : RC = " << sockFd << endl;
        return returnCode;
    }
    cout << "Success in creating the socket : RC = " << sockFd << endl;

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
        cerr << "Failed to connect to socket : RC = " << returnCode << endl;
        return returnCode;
    }
    cout << "Success in connecting to socket : RC = " << returnCode << endl;

    auto pldmType = MCTP_MSG_TYPE_PLDM;
    result = write(socketFd(), &pldmType, sizeof(pldmType));
    if (-1 == result)
    {
        returnCode = -errno;
        cerr << "Failed to send message type as pldm to mctp : RC = "
             << returnCode << endl;
        return returnCode;
    }
    cout << "Success in sending message type as pldm to mctp : RC = "
         << returnCode << endl;

    result = send(socketFd(), requestMsg.data(), requestMsg.size(), 0);
    if (-1 == result)
    {
        returnCode = -errno;
        cerr << "Write to socket failure : RC = " << returnCode << endl;
        return returnCode;
    }
    cout << "Write to socket successful : RC = " << result << endl;

    // Read the response from socket
    ssize_t peekedLength = recv(socketFd(), nullptr, 0, MSG_TRUNC | MSG_PEEK);
    if (0 == peekedLength)
    {
        cerr << "Socket is closed : peekedLength = " << peekedLength << endl;
        return returnCode;
    }
    else if (peekedLength <= -1)
    {
        returnCode = -errno;
        cerr << "recv() system call failed : RC = " << returnCode << endl;
        return returnCode;
    }
    else
    {
        // loopback response message
        std::vector<uint8_t> loopBackRespMsg(peekedLength);
        auto recvDataLength =
            recv(socketFd(), reinterpret_cast<void*>(loopBackRespMsg.data()),
                 peekedLength, 0);
        if (recvDataLength == peekedLength)
        {
            cout << "Total length:" << recvDataLength << endl;
            cout << "Loopback response message:" << endl;
            printBuffer(loopBackRespMsg);
        }
        else
        {
            cerr << "Failure to read peeked length packet : RC = " << returnCode
                 << endl;
            cerr << "peekedLength: " << peekedLength << endl;
            cerr << "Total length: " << recvDataLength << endl;
            return returnCode;
        }

        // Confirming on the first recv() the Request bit is set in
        // pldm_msg_hdr struct. If set proceed with recv() or else, quit.
        auto hdr = reinterpret_cast<const pldm_msg_hdr*>(&loopBackRespMsg[2]);
        uint8_t request = hdr->request;
        if (request == PLDM_REQUEST)
        {
            cout << "On first recv(),response == request : RC = " << returnCode
                 << endl;
            ssize_t peekedLength =
                recv(socketFd(), nullptr, 0, MSG_PEEK | MSG_TRUNC);

            responseMsg.resize(peekedLength);
            recvDataLength =
                recv(socketFd(), reinterpret_cast<void*>(responseMsg.data()),
                     peekedLength, 0);
            if (recvDataLength == peekedLength)
            {
                cout << "Total length: " << recvDataLength << endl;
            }
            else
            {
                cerr << "Failure to read response length packet: length = "
                     << recvDataLength << endl;
                return returnCode;
            }
        }
        else
        {
            cerr << "On first recv(),request != response : RC = " << returnCode
                 << endl;
            return returnCode;
        }
    }
    returnCode = shutdown(socketFd(), SHUT_RDWR);
    if (-1 == returnCode)
    {
        returnCode = -errno;
        cerr << "Failed to shutdown the socket : RC = " << returnCode << endl;
        return returnCode;
    }

    cout << "Shutdown Socket successful :  RC = " << returnCode << endl;
    return PLDM_SUCCESS;
}

void CommandInterface::exec()
{
    auto [rc, requestMsg] = createRequestMsg();
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Failed to encode request message for " << pldmType << ":"
                  << commandName << " rc = " << rc << std::endl;
        return;
    }

    std::cout << "Encode request successfully" << std::endl;

    // Insert the PLDM message type and EID at the begining of the request msg.
    requestMsg.insert(requestMsg.begin(), MCTP_MSG_TYPE_PLDM);
    requestMsg.insert(requestMsg.begin(), PLDM_ENTITY_ID);

    std::cout << "Request Message:" << std::endl;
    printBuffer(requestMsg);

    std::vector<uint8_t> responseMsg;
    rc = mctpSockSendRecv(requestMsg, responseMsg);

    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Failed to receive from socket: RC = " << rc << std::endl;
        return;
    }

    std::cout << "Response Message:" << std::endl;
    printBuffer(requestMsg);

    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg.data());
    parseResponseMsg(responsePtr, responseMsg.size());
}