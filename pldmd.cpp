#include "libpldmresponder/base.hpp"
#include "libpldmresponder/bios.hpp"
#include "libpldmresponder/utils.hpp"
#include "registration.hpp"

#include <err.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <iterator>
#include <phosphor-logging/log.hpp>
#include <sstream>
#include <vector>

#include "libpldm/base.h"
#include "libpldm/bios.h"

#ifdef OEM_IBM
#include "libpldmresponder/file_io.hpp"
#endif

constexpr uint8_t MCTP_MSG_TYPE_PLDM = 1;

using namespace phosphor::logging;
using namespace pldm;

static Response processRxMsg(std::vector<uint8_t> const& requestMsg)
{

    Response response;
    uint8_t eid = requestMsg.data()[0];
    uint8_t type = requestMsg.data()[1];
    pldm_header_info hdrFields{};
    pldm_msg_hdr* hdr = reinterpret_cast<pldm_msg_hdr*>(
        const_cast<uint8_t*>(requestMsg.data()) + sizeof(eid) + sizeof(type));
    if (PLDM_SUCCESS != unpack_pldm_header(hdr, &hdrFields))
    {
        log<level::ERR>("Invalid PLDM request header");
    }
    else
    {
        pldm_msg* request = reinterpret_cast<pldm_msg*>(hdr);
        size_t requestLen = requestMsg.size() - sizeof(eid) - sizeof(type);
        response = pldm::responder::invokeHandler(
            hdrFields.pldm_type, hdrFields.command, request, requestLen);
        if (!response.empty())
        {
            response.insert(response.begin(), type);
            response.insert(response.begin(), eid);
        }
    }
    return response;
}

void printBuffer(std::vector<uint8_t> const& buffer)
{
    std::ostringstream tempStream;
    tempStream << "Buffer Data: ";
    if (!buffer.empty())
    {
        std::copy(buffer.begin(), buffer.end(),
                  std::ostream_iterator<int>(tempStream, " "));
    }
    log<level::INFO>(tempStream.str().c_str());
}

int main(int argc, char** argv)
{

    pldm::responder::base::registerHandlers();
    pldm::responder::bios::registerHandlers();

#ifdef OEM_IBM
    pldm::responder::oem_ibm::registerHandlers();
#endif

    /* Create local socket. */
    int sockfd = -1;
    int returnCode = 0;
    sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (-1 == sockfd)
    {
        returnCode = -errno;
        log<level::ERR>("Failed to create the socket",
                        entry("RC=%d", returnCode));
        exit(EXIT_FAILURE);
    }

    responder::utils::CustomFD socketFd(sockfd);

    struct sockaddr_un addr
    {
    };
    addr.sun_family = AF_UNIX;
    const char path[] = "\0mctp-mux";
    memcpy(addr.sun_path, path, sizeof(path) - 1);
    int result = -1;
    result = connect(socketFd(), reinterpret_cast<struct sockaddr*>(&addr),
                     sizeof(path) + sizeof(addr.sun_family) - 1);
    if (-1 == result)
    {
        returnCode = -errno;
        log<level::ERR>("Failed to connect to the socket",
                        entry("RC=%d", returnCode));
        exit(EXIT_FAILURE);
    }

    auto pldmType = MCTP_MSG_TYPE_PLDM;
    result = write(socketFd(), &pldmType, sizeof(pldmType));
    if (-1 == result)
    {
        returnCode = -errno;
        log<level::ERR>("Failed to send message type as pldm to mctp",
                        entry("RC=%d", returnCode));
        exit(EXIT_FAILURE);
    }

#ifdef VERBOSE
    log<level::INFO>("Started and connected successfully.");
#endif

    do
    {
        std::array<uint8_t, 64> recvBuf{};
        ssize_t peekedLength =
            recv(socketFd(), reinterpret_cast<void*>(recvBuf.data()),
                 recvBuf.size(), MSG_PEEK);
        if (0 == peekedLength)
        {
            log<level::ERR>("Socket has been closed");
            exit(EXIT_FAILURE);
        }
        else if (peekedLength <= -1)
        {
            returnCode = -errno;
            log<level::ERR>("recv system call failed",
                            entry("RC=%d", returnCode));
            exit(EXIT_FAILURE);
        }
        else
        {
            std::vector<uint8_t> requestMsg(peekedLength);
            auto recvDataLength =
                recv(sockfd, reinterpret_cast<void*>(requestMsg.data()),
                     peekedLength, 0);
            if (recvDataLength == peekedLength)
            {
#ifdef VERBOSE
                log<level::INFO>(" Received Msg ",
                                 entry("length=%zu", recvDataLength),
                                 entry("eid=%d", requestMsg.data()[0]),
                                 entry("type=%d", requestMsg.data()[1]));
                printBuffer(requestMsg);
#endif
                if (pldmType != requestMsg.data()[1])
                {
                    // Skip this message and continue.
                    log<level::ERR>("Encountered Non-PLDM type message",
                                    entry("Type=%d", requestMsg.data()[1]));
                }
                else
                {
                    // process message and send response
                    auto response = processRxMsg(requestMsg);
                    if (!response.empty())
                    {
#ifdef VERBOSE
                        log<level::INFO>(" Sending Msg ");
                        printBuffer(response);
#endif
                        result = sendto(socketFd(), response.data(),
                                        response.size(), 0, nullptr, 0);
                        if (-1 == result)
                        {
                            returnCode = -errno;
                            log<level::ERR>("sendto system call failed",
                                            entry("RC=%d", returnCode));
                            exit(EXIT_FAILURE);
                        }
                    }
                }
            }
            else
            {
                log<level::ERR>("Failure to read peeked length packet");
                exit(EXIT_FAILURE);
            }
        }
    } while (true);

    result = shutdown(sockfd, SHUT_RDWR);
    if (-1 == result)
    {
        returnCode = -errno;
        log<level::ERR>("Failed to shutdown the socket",
                        entry("RC=%d", returnCode));
        exit(EXIT_FAILURE);
    }
    exit(EXIT_FAILURE);
}
