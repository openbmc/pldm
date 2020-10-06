#include "utils.hpp"

#include "libpldm/base.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <iostream>

namespace pldm
{
namespace responder
{
namespace utils
{

int setupUnixSocket(const std::string& socketInterface)
{
    int sock;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (strnlen(socketInterface.c_str(), sizeof(addr.sun_path)) ==
        sizeof(addr.sun_path))
    {
        std::cerr << "setupUnixSocket: UNIX socket path too long" << std::endl;
        return PLDM_ERROR;
    }

    strncpy(addr.sun_path, socketInterface.c_str(), sizeof(addr.sun_path) - 1);

    if ((sock = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1)
    {
        std::cerr << "setupUnixSocket: socket() call failed" << std::endl;
        return PLDM_ERROR;
    }

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        std::cerr << "setupUnixSocket: bind() call failed" << std::endl;
        return PLDM_ERROR;
    }

    if (listen(sock, 1) == -1)
    {
        std::cerr << "setupUnixSocket: listen() call failed" << std::endl;
        return PLDM_ERROR;
    }

    fd_set rfd;
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    FD_ZERO(&rfd);
    FD_SET(sock, &rfd);
    int nfd = sock + 1;

    int retval = select(nfd, &rfd, NULL, NULL, &tv);
    if ((retval < 0) && (errno != EINTR))
    {
        std::cerr << "setupUnixSocket: select call failed " << errno
                  << std::endl;
        return PLDM_ERROR;
    }

    if ((retval > 0) && (FD_ISSET(sock, &rfd)))
    {

        int fd = accept(sock, NULL, NULL);
        if (fd < 0)
        {
            std::cout << "accept failed" << std::endl;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                std::cerr << "setupUnixSocket: accept() call failed " << errno
                          << std::endl;
            }
            return PLDM_ERROR;
        }
        sock = fd;
    }
    return sock;
}

int writeOnUnixSocket(const int sock, const char* buf, const uint64_t blockSize)
{
    uint64_t i;
    int nwrite = 0;

    for (i = 0; i < blockSize; i = i + nwrite)
    {

        fd_set wfd;
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        FD_ZERO(&wfd);
        FD_SET(sock, &wfd);
        int nfd = sock + 1;

        int retval = select(nfd, NULL, &wfd, NULL, &tv);
        if ((retval < 0) && (errno != EINTR))
        {
            std::cerr << "writeOnUnixSocket: select call failed " << errno
                      << std::endl;
            return PLDM_ERROR;
        }
        if (retval == 0)
        {
            nwrite = 0;
            continue;
        }
        if ((retval > 0) && (FD_ISSET(sock, &wfd)))
        {
            nwrite = write(sock, buf + i, blockSize - i);
            if (nwrite < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                {
                    std::cerr << "writeOnUnixSocket: Write call failed with "
                                 "EAGAIN or EWOULDBLOCK or EINTR"
                              << std::endl;
                    nwrite = 0;
                    continue;
                }
                std::cerr << "writeOnUnixSocket: Failed to write" << std::endl;
                return PLDM_ERROR;
            }
        }
        else
        {
            nwrite = 0;
        }
    }
    return PLDM_SUCCESS;
}

} // namespace utils
} // namespace responder
} // namespace pldm
