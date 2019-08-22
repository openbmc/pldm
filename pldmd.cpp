#include "libpldmresponder/base.hpp"
#include "libpldmresponder/bios.hpp"
#include "libpldmresponder/platform.hpp"
#include "libpldmresponder/utils.hpp"
#include "registration.hpp"

#include <err.h>
#include <getopt.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <phosphor-logging/log.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "libpldm/base.h"
#include "libpldm/bios.h"
#include "libpldm/platform.h"

#ifdef OEM_IBM
#include "libpldmresponder/file_io.hpp"
#include "libpldmresponder/file_io_by_type.hpp"
#endif

constexpr uint8_t MCTP_MSG_TYPE_PLDM = 1;

using namespace phosphor::logging;
using namespace pldm;

static Response processRxMsg(const std::vector<uint8_t>& requestMsg)
{

    Response response;
    uint8_t eid = requestMsg[0];
    uint8_t type = requestMsg[1];
    pldm_header_info hdrFields{};
    auto hdr = reinterpret_cast<const pldm_msg_hdr*>(
        requestMsg.data() + sizeof(eid) + sizeof(type));
    if (PLDM_SUCCESS != unpack_pldm_header(hdr, &hdrFields))
    {
        log<level::ERR>("Empty PLDM request header");
    }
    else if (PLDM_RESPONSE != hdrFields.msg_type)
    {
        auto request = reinterpret_cast<const pldm_msg*>(hdr);
        size_t requestLen = requestMsg.size() - sizeof(struct pldm_msg_hdr) -
                            sizeof(eid) - sizeof(type);
        response = pldm::responder::invokeHandler(
            hdrFields.pldm_type, hdrFields.command, request, requestLen);
        if (response.empty())
        {
            uint8_t completion_code = PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
            response.resize(sizeof(pldm_msg_hdr));
            auto responseHdr = reinterpret_cast<pldm_msg_hdr*>(response.data());
            pldm_header_info header{};
            header.msg_type = PLDM_RESPONSE;
            header.instance = hdrFields.instance;
            header.pldm_type = hdrFields.pldm_type;
            header.command = hdrFields.command;
            auto result = pack_pldm_header(&header, responseHdr);
            if (PLDM_SUCCESS != result)
            {
                log<level::ERR>("Failed adding response header");
            }
            response.insert(response.end(), completion_code);
        }
    }
    return response;
}

void printBuffer(const std::vector<uint8_t>& buffer)
{
    std::ostringstream tempStream;
    tempStream << "Buffer Data: ";
    if (!buffer.empty())
    {
        for (int byte : buffer)
        {
            tempStream << std::setfill('0') << std::setw(2) << std::hex << byte
                       << " ";
        }
    }
    log<level::INFO>(tempStream.str().c_str());
}

void optionUsage(void)
{
    std::cerr << "Usage: pldmd [options]\n";
    std::cerr << "Options:\n";
    std::cerr
        << "  --verbose=<0/1>  0 - Disable verbosity, 1 - Enable verbosity\n";
    std::cerr << "Defaulted settings:  --verbose=0 \n";
}

int main(int argc, char** argv)
{

    bool verbose = false;
    static struct option long_options[] = {
        {"verbose", required_argument, 0, 'v'}, {0, 0, 0, 0}};

    auto argflag = getopt_long(argc, argv, "v:", long_options, nullptr);
    switch (argflag)
    {
        case 'v':
            switch (std::stoi(optarg))
            {
                case 0:
                    verbose = false;
                    break;
                case 1:
                    verbose = true;
                    break;
                default:
                    optionUsage();
                    break;
            }
            break;
        default:
            optionUsage();
            break;
    }

    pldm::responder::base::registerHandlers();
    pldm::responder::bios::registerHandlers();
    pldm::responder::platform::registerHandlers();

    // Outgoing message.
    struct iovec iov[2]{};

    // This structure contains the parameter information for the response
    // message.
    struct msghdr msg
    {
    };

#ifdef OEM_IBM
    pldm::responder::oem_ibm::registerHandlers();
#endif

    /* Create local socket. */
    int returnCode = 0;
    int sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
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
    int result = connect(socketFd(), reinterpret_cast<struct sockaddr*>(&addr),
                         sizeof(path) + sizeof(addr.sun_family) - 1);
    if (-1 == result)
    {
        returnCode = -errno;
        log<level::ERR>("Failed to connect to the socket",
                        entry("RC=%d", returnCode));
        exit(EXIT_FAILURE);
    }

    result = write(socketFd(), &MCTP_MSG_TYPE_PLDM, sizeof(MCTP_MSG_TYPE_PLDM));
    if (-1 == result)
    {
        returnCode = -errno;
        log<level::ERR>("Failed to send message type as pldm to mctp",
                        entry("RC=%d", returnCode));
        exit(EXIT_FAILURE);
    }

    do
    {
        ssize_t peekedLength =
            recv(socketFd(), nullptr, 0, MSG_PEEK | MSG_TRUNC);
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
            auto recvDataLength = recv(
                sockfd, static_cast<void*>(requestMsg.data()), peekedLength, 0);
            if (recvDataLength == peekedLength)
            {
                if (verbose)
                {
                    log<level::INFO>("Received Msg ",
                                     entry("LENGTH=%zu", recvDataLength),
                                     entry("EID=0x%02x", requestMsg[0]),
                                     entry("TYPE=0x%02x", requestMsg[1]));
                    printBuffer(requestMsg);
                }
                if (MCTP_MSG_TYPE_PLDM != requestMsg[1])
                {
                    // Skip this message and continue.
                    log<level::ERR>("Encountered Non-PLDM type message",
                                    entry("TYPE=0x%02x", requestMsg[1]));
                }
                else
                {
                    // process message and send response
                    auto response = processRxMsg(requestMsg);
                    if (!response.empty())
                    {
                        if (verbose)
                        {
                            log<level::INFO>("Sending Msg ");
                            printBuffer(response);
                        }

                        iov[0].iov_base = &requestMsg[0];
                        iov[0].iov_len =
                            sizeof(requestMsg[0]) + sizeof(requestMsg[1]);
                        iov[1].iov_base = response.data();
                        iov[1].iov_len = response.size();

                        msg.msg_iov = iov;
                        msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);

                        result = sendmsg(socketFd(), &msg, 0);
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
                log<level::ERR>("Failure to read peeked length packet",
                                entry("PEEKED_LENGTH=%zu", peekedLength),
                                entry("READ_LENGTH=%zu", recvDataLength));
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
