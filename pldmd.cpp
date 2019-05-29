#include "libpldmresponder/base.hpp"
#include "libpldmresponder/bios.hpp"
#include "libpldmresponder/file_io.hpp"
#include "libpldmresponder/platform.hpp"
#include "registration.hpp"

#include <err.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <phosphor-logging/log.hpp>

#include "libpldm/base.h"
#include "libpldm/bios.h"
#include "libpldm/file_io.h"
#include "libpldm/platform.h"

#define PLDM_TYPE_MESSAGE 0x01

using namespace phosphor::logging;

const char path[] = "\0mctp-mux";
int sockfd = 0;

std::map<uint8_t, size_t> commandToRespSize = {
    {PLDM_GET_PLDM_VERSION, PLDM_GET_VERSION_RESP_BYTES},
    {PLDM_GET_PLDM_TYPES, PLDM_GET_TYPES_RESP_BYTES},
    {PLDM_GET_PLDM_COMMANDS, PLDM_GET_COMMANDS_RESP_BYTES},
    {PLDM_SET_STATE_EFFECTER_STATES, PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES},
    {PLDM_READ_FILE_INTO_MEMORY, PLDM_RW_FILE_MEM_RESP_BYTES},
    {PLDM_WRITE_FILE_FROM_MEMORY, PLDM_RW_FILE_MEM_RESP_BYTES},
    {PLDM_GET_DATE_TIME, PLDM_GET_DATE_TIME_RESP_BYTES}};

static void processRxMsg(uint8_t eid, void* data, size_t len)
{
    uint8_t type = PLDM_TYPE_MESSAGE;
    pldm_header_info hdrFields{};
    pldm_msg_hdr* hdr = static_cast<pldm_msg_hdr*>(data);
    if (PLDM_SUCCESS != unpack_pldm_header(hdr, &hdrFields))
    {
        log<level::ERR>("Invalid PLDM request");
    }
    else
    {
        pldm_msg_payload body{};
        body.payload = static_cast<uint8_t*>(data) + sizeof(pldm_msg_hdr);
        body.payload_length = len - sizeof(pldm_msg_hdr);
        int responseMsgLength = sizeof(eid) + sizeof(type) +
                                sizeof(pldm_msg_hdr) +
                                commandToRespSize[hdrFields.command];
        std::vector<uint8_t> responseMsg{};
        responseMsg.resize(responseMsgLength);
        responseMsg[0] = eid;
        responseMsg[1] = PLDM_TYPE_MESSAGE;
        pldm_msg response{};
        response.body.payload = sizeof(eid) + sizeof(type) +
                                sizeof(pldm_msg_hdr) + responseMsg.data();
        response.body.payload_length = commandToRespSize[hdrFields.command];
        pldm::responder::invokeHandler(hdrFields.pldm_type, hdrFields.command,
                                       &body, &response);
        memcpy(responseMsg.data() + sizeof(eid) + sizeof(type), &response,
               sizeof(pldm_msg_hdr));

        // Send the response.
        // send();
        //----------------------------------------------------------
#ifdef PLDMVERBOSE
        fprintf(stdout,
                " PLDM Daemon :: Response Payload length [%d]   Response "
                "Message Length  [%d]  pldm type [%d] pldm command [%d]  pldm "
                "instance [%d]\n",
                response.body.payload_length, responseMsgLength,
                hdrFields.pldm_type, hdrFields.command, hdrFields.instance);
        fprintf(stdout, " PLDM Daemon :: Sending Data: ");
        for (int i = 0; i < responseMsgLength; i++)
        {
            fprintf(stdout, "%02x ", *(responseMsg.data() + i));
        }
        fprintf(stdout, "\n");
#endif
        //----------------------------------------------------------
        sendto(sockfd, responseMsg.data(), responseMsgLength, 0, NULL, 0);
    }
}

int main(int argc, char** argv)
{

    pldm::responder::base::registerHandlers();
    pldm::responder::platform::registerHandlers();
    pldm::responder::fileio::registerHandlers();
    pldm::responder::bios::registerHandlers();

    struct sockaddr_un addr;
    const int pldmType = PLDM_TYPE_MESSAGE;
    int result = -1;

    /*  -AF_UNIX is a UNIX internal socket, created on the filesystem.
     *  -SOCK_SEQPACKET is a reliable, sequenced, connection-based
     *  two-way byte stream, will return only the amount of data requested,
     *  and any data remaining in the arriving packet will be discarded.
     */
    /* Create local socket. */
    sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    /* -sun_family - specifies type (domain) of socket
     * -sun_path - socket path location (on filesystem) */
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, path, sizeof(path) - 1);

    /*  Request connection from the server, connecting socket
     *  to server with specified address
     */
    result = connect(sockfd, (struct sockaddr*)(&addr),
                     sizeof(path) + sizeof(addr.sun_family) - 1);
    if (-1 == result)
    {
        /* exit with error if connection is not successful */
        close(sockfd);
        err(EXIT_FAILURE,
            "PLDM Daemon :: The server is down. Connection failure");
    }

    result = write(sockfd, &pldmType, 1);
    if (-1 == result)
    {
        close(sockfd);
        err(EXIT_FAILURE, "Failed sending Type.");
    }

    log<level::DEBUG>("PLDM Daemon :: Started and connected successfully.");
    // poll for incomming messages
    // use the poll system call to be notified about socket status changes
    struct pollfd pollsockfd;
    pollsockfd.fd = sockfd;
    pollsockfd.events = POLLIN | POLLHUP | POLLRDNORM;
    pollsockfd.revents = 0;
    while (0 == pollsockfd.revents)
    {
        // call poll with a timeout of 100 ms
        if (poll(&pollsockfd, 1, 100) > 0)
        {
            // if result > 0, this means that there is either data available on
            // the socket, or the socket has been closed
            int peekedLength = 0;
            char buf[64];
            peekedLength = recv(sockfd, buf, 64, MSG_PEEK | MSG_DONTWAIT);
            if (peekedLength == 0)
            {
                // if recv returns ZERO, that means the connection has been
                // closed
                close(sockfd);
                err(EXIT_FAILURE, "PLDM Daemon :: Closed socket encountered. ");
            }
            else if (peekedLength <= -1)
            {
                // The recv has returned an error
                close(sockfd);
                err(EXIT_FAILURE, "PLDM Daemon :: Receive failed. ");
            }
            else
            {
                auto recvDataBuffer = malloc(peekedLength);
                auto recvDataLength =
                    recv(sockfd, recvDataBuffer, peekedLength, 0);
                if (recvDataLength == peekedLength)
                {

                    // msg content will start with an 8-bit eid followed by an
                    // 8-bit type (indicating the type of message it will be
                    // 0x01 for PLDM messages) which will be followed by the
                    // message. The message contents will be formulated as per
                    // the PLDM specification regarding the specific message.
                    uint8_t eid = *((uint8_t*)recvDataBuffer);
                    uint8_t type = *((uint8_t*)recvDataBuffer + 1);

                    //----------------------------------------------------------
#ifdef PLDMVERBOSE
                    fprintf(stdout,
                            " PLDM Daemon :: Total length [%d] EID [%d] "
                            "Message Type [%d]. \n",
                            recvDataLength, eid, type);
                    fprintf(stdout, " PLDM Daemon :: Recieved Data:");
                    for (int i = 0; i < recvDataLength; i++)
                    {
                        fprintf(stdout, "%02x ", *((char*)recvDataBuffer + i));
                    }
                    fprintf(stdout, "\n");
#endif
                    //----------------------------------------------------------

                    if (PLDM_TYPE_MESSAGE != type)
                    {
                        // Exiting here as we have registered for PLDM type
                        // messages only and should not be getting any other
                        // type of messages.
                        err(EXIT_FAILURE,
                            "PLDM Daemon :: Type Error. Not a PLDM message. "
                            "EID:[%d] Type [%d]",
                            eid, type);
                    }
                    int msgLength = recvDataLength - sizeof(eid) - sizeof(type);
                    void* msgPointer =
                        (uint8_t*)recvDataBuffer + sizeof(eid) + sizeof(type);
                    processRxMsg(eid, msgPointer, msgLength);
                }
                else
                {
                    err(EXIT_FAILURE,
                        "PLDM Daemon :: Receive failed. Peeked Length [%d]. "
                        "Read Length [%d]",
                        peekedLength, recvDataLength);
                }
                pollsockfd.revents = 0;
            }
        }
    }
    shutdown(sockfd, SHUT_RDWR);
    result = close(sockfd);
    if (result == -1)
    {
        /* exit with error if it is not successful */
        err(EXIT_FAILURE, "PLDM Daemon :: Failed to close Socket. ");
    }
    return 0;
}
