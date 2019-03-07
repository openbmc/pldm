#include "pldmtool-receiver.hpp"

#include "registration.hpp"

#include <assert.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>

#include "libmctp/libmctp-serial.h"
#include "libmctp/libmctp.h"
#include "libpldm/base.h"

namespace pldm
{
namespace tool
{
namespace receiver
{

namespace internal
{

int fdIn;
int fdOut;
struct mctp_binding_serial* serial[2]{};
struct mctp* mctp[2]{};

} // namespace internal

static int callback(sd_event_source* s, int fd, uint32_t revents,
                    void* userdata)
{
    using namespace internal;

    if (revents & EPOLLIN)
    {
        struct mctp_binding_serial* serial =
            static_cast<struct mctp_binding_serial*>(userdata);
        auto rc = mctp_serial_read(serial);
        if (rc)
        {
            return -1;
        }
    }

    return 0;
}

static void rxCallback(uint8_t eid, void* data, void* msg, size_t len)
{
    using namespace internal;

    (void)eid;
    (void)data;

    pldm_header_info hdrFields{};
    pldm_msg_hdr* hdr = static_cast<pldm_msg_hdr*>(msg);
    if (PLDM_SUCCESS != unpack_pldm_header(hdr, &hdrFields))
    {
        fprintf(stderr, "Invalid PLDM request");
    }
    else
    {
        pldm_msg_payload body{};
        body.payload = static_cast<uint8_t*>(msg) + sizeof(pldm_msg_hdr);
        body.payload_length = len - sizeof(pldm_msg_hdr);

        std::array<uint8_t, PLDM_GET_COMMANDS_RESP_BYTES + sizeof(pldm_msg_hdr)>
            responseMsg{};
        pldm_msg response{};
        response.body.payload = responseMsg.data() + sizeof(pldm_msg_hdr);
        response.body.payload_length = PLDM_GET_COMMANDS_RESP_BYTES;

        pldm::responder::invokeHandler(hdrFields.pldm_type, hdrFields.command,
                                       &body, &response);
        memcpy(responseMsg.data(), &response, sizeof(pldm_msg_hdr));
        mctp_message_tx(mctp[1], 0, responseMsg.data(),
                        PLDM_GET_COMMANDS_RESP_BYTES + sizeof(pldm_msg_hdr));
    }
}

void setup(sd_event* loop)
{
    using namespace internal;

    mctp[0] = mctp_init();
    assert(mctp[0]);
    mctp[1] = mctp_init();
    assert(mctp[1]);

    serial[0] = mctp_serial_init();
    assert(serial[0]);
    serial[1] = mctp_serial_init();
    assert(serial[1]);

    auto requests = "/dev/pldmin";
    auto responses = "/dev/pldmout";
    auto rc = mkfifo(requests, 0666);
    assert(!rc);
    rc = mkfifo(responses, 0666);
    assert(!rc);

    fdIn = open(requests, O_RDONLY);
    mctp_serial_open_fd(serial[0], fdIn);
    mctp_serial_register_bus(serial[0], mctp[0], 0);

    fdOut = open(responses, O_WRONLY);
    mctp_serial_open_fd(serial[1], fdOut);
    mctp_serial_register_bus(serial[1], mctp[1], 0);

    mctp_set_rx_all(mctp[0], rxCallback, NULL);

    rc = sd_event_add_io(loop, nullptr, fdIn, EPOLLIN, callback, serial[0]);
    assert(rc >= 0);
}

void teardown()
{
    using namespace internal;

    close(fdIn);
    close(fdOut);
    remove("/dev/pldmin");
    remove("/dev/pldmout");
}

} // namespace receiver
} // namespace tool
} // namespace pldm
