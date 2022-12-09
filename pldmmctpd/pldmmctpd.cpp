#include "libpldm/pldm_base_requester.h"
#include "libpldm/base.h"
#include "libpldm/pldm.h"
#include "mctp.h"

#include "pldm_base_helper.hpp"
#include "requester/handler.hpp"

#include <string.h>

#include <sdeventplus/event.hpp>

#include <iostream>
#include <vector>

using namespace sdeventplus;

int main()
{
    std::cout << "Starting the PLDM MCTP Requester Daemon \n";
    int fd = -1;
    int rc = -1;
    fd = socket(AF_MCTP, SOCK_DGRAM, 0);

    if (-1 == fd)
    {
        std::cerr << "Socket creation failed with error code: "
                  << strerror(errno) << "\n";
        return fd;
    }
    sleep(5);

    auto& bus = pldm::utils::DBusHandler::getBus();
    sdbusplus::server::manager_t objManager(
        bus, "/xyz/openbmc_project/pldm/rde/request");

    socklen_t optlen;
    int currentSendbuffSize;
    int res =
        getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &currentSendbuffSize, &optlen);
    if (res == -1)
    {
        std::cerr << "Error in obtaining the default send buffer size, Error : "
                  << strerror(errno) << std::endl;
    }

    optlen = sizeof(currentSendbuffSize);
    sleep(2);
    auto event = Event::get_default();

    // TODO: Define bus interfaces
    pldm::dbus_api::Requester dbusImplReq(
        bus, "/xyz/openbmc_project/pldm/rde/request");

    // Initialize the requester context
    struct requester_base_context* ctx = (struct requester_base_context*)malloc(
        sizeof(struct requester_base_context));

    uint8_t eid = (uint8_t)9;
    rc = pldm_base_start_discovery(ctx);
    if (-1 == rc)
    {
        std::cerr << "Error in initializing RDE Requester Context, Error : "
                  << strerror(errno) << "\n";
    }
    std::cout << "Context Initialized: " << ctx->initialized << "\n";

    // Process base discovery requests
    while (true)
    {
        int requestBytes;
        if (command_request_size.find(ctx->next_command) !=
            command_request_size.end())
        {
            requestBytes = command_request_size[ctx->next_command];
        }
        else
        {
            requestBytes = PLDM_MAX_REQUEST_BYTES;
        }

        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) + requestBytes);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        std::cerr << "Getting next request...\n";
        rc = pldm_base_get_next_request(ctx, 1, request);
        if (rc)
        {
            std::cerr << "No more requests to process\n";
            break;
        }
        rc = process_next_request(fd, eid, 1, ctx, requestMsg);
        if (rc)
        {
            std::cerr << "Failure in processing request\n";
            break;
        }
    }

    // TODO: Run RDE Discovery Requests

    // TODO: Remove this afterwards
    printContext(ctx);

    int returnCode = event.loop();
    if (returnCode)
    {
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
