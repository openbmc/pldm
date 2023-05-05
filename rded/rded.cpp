#include <locale.h>
#include <unistd.h>
#include <cstddef>

#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <iostream>
#include <vector>

#include <sdeventplus/event.hpp>
#include "common/utils.hpp"
using namespace sdeventplus;
using namespace pldm::utils;

// MCTP Socket for RDE communication
int fd;

void set_socket_timeout(int fd, int seconds, int milliseconds)
{
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = milliseconds;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
}

int main()
{
    int rc;
    std::cerr << "Successfully started the RDE Daemon \n";
    fd = socket(AF_MCTP, SOCK_DGRAM, 0);
    if (-1 == fd)
    {
        std::cerr << "Created socket Failed\n";
        return fd;
    }

    std::cerr << "Socket Created with ID: " << fd << ". Setting timeouts...\n";
    set_socket_timeout(fd, /*seconds=*/5, /*milliseconds=*/0);
    socklen_t optlen;
    int currentSendbuffSize;
    optlen = sizeof(currentSendbuffSize);

    int res = getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &currentSendbuffSize, &optlen);
    if (res == -1)
    {
        std::cerr << "Error in obtaining the default send buffer size, Error: "
                  << strerror(errno) << std::endl;
        return -1;
    }

    // TODO: Use PDR P&M PLDM Type to create Resource id mapping

    // TODO (@harstya): Create a RDE Reactor to react to RDE Devices detected

    // Daemon loop
    auto event = Event::get_default();
    rc = event.loop();
    if (rc)
    {
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

