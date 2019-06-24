#include "pldm_cmd_helper.hpp"

constexpr uint8_t MCTP_MSG_TYPE_PLDM = 1;

/*
 * Initialize the socket to send & recieve response from socket
 *
 */
int mctp_mux_init()
{
    const char devPath[] = "\0mctp-mux";
    struct sockaddr_un addr
    {
    };
    addr.sun_family = AF_UNIX;
    int returnCode = 0;
    int result = -1;
    int sockfd = -1;

    sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (-1 == sockfd)
    {
        returnCode = -errno;
        Logger::Error("Failed to create the socket : RC = [%d]", returnCode);
        return returnCode;
    }
    Logger::Debug("Success in creating the socket : RC = [%d]", returnCode);

    memcpy(addr.sun_path, devPath, sizeof(devPath) - 1);

    result = connect(sockfd, reinterpret_cast<struct sockaddr*>(&addr),
                     sizeof(devPath) + sizeof(addr.sun_family) - 1);
    if (-1 == result)
    {
        returnCode = -errno;
        Logger::Error("Failed to connect to the socket : RC = [%d]",
                      returnCode);
        close(sockfd);
        return returnCode;
    }
    Logger::Debug("Success in connecting to the socket : RC = [%d]",
                  returnCode);

    auto pldmType = MCTP_MSG_TYPE_PLDM;
    result = write(sockfd, &pldmType, sizeof(pldmType));
    if (-1 == result)
    {
        returnCode = -errno;
        Logger::Error("Failed to send message type as pldm to mctp : RC = [%d]",
                      returnCode);
        close(sockfd);
        return returnCode;
    }
    Logger::Debug("Success in sending message type as pldm to mctp : RC = [%d]",
                  returnCode);

    return sockfd;
}
