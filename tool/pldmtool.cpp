#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>

#include "libmctp/libmctp-serial.h"
#include "libmctp/libmctp.h"
#include "libpldm/base.h"

static void rxCallback(uint8_t eid, void* data, void* msg, size_t len)
{
    (void)eid;
    (void)data;
    pldm_msg_payload body{};
    body.payload = static_cast<uint8_t*>(msg) + sizeof(pldm_msg_hdr);
    body.payload_length = len - sizeof(pldm_msg_hdr);
    uint8_t cc{};
    std::array<bitfield8_t, PLDM_MAX_CMDS_PER_TYPE / 8> commands{};
    decode_get_commands_resp(&body, &cc, commands.data());
    printf("Completion Code : %d\n", cc);
    printf("Supported Commands bitfield : 0x%.2X\n", commands[0].byte);
}

int main(int argc, char** argv)
{
    struct mctp* mctp[2];
    mctp[0] = mctp_init();
    assert(mctp[0]);
    mctp[1] = mctp_init();
    assert(mctp[1]);

    struct mctp_binding_serial* serial[2];
    serial[0] = mctp_serial_init();
    assert(serial[0]);
    serial[1] = mctp_serial_init();
    assert(serial[1]);

    auto requests = "/dev/pldmin";
    auto responses = "/dev/pldmout";

    auto fdOut = open(requests, O_WRONLY);
    mctp_serial_open_fd(serial[0], fdOut);
    mctp_serial_register_bus(serial[0], mctp[0], 0);

    auto fdIn = open(responses, O_RDONLY);
    mctp_serial_open_fd(serial[1], fdIn);
    mctp_serial_register_bus(serial[1], mctp[1], 0);

    mctp_set_rx_all(mctp[1], rxCallback, NULL);

    if (argc != 3)
    {
        err(EXIT_FAILURE, "unsupported command");
    }
    if (std::string(argv[1]) != "getpldmcommands")
    {
        err(EXIT_FAILURE, "unsupported command");
    }
    if (std::string(argv[2]) != "base")
    {
        err(EXIT_FAILURE, "unsupported pldm type");
    }

    uint8_t pldmType = 0;
    ver32_t version{0xFF, 0xFF, 0xFF, 0xFF};
    std::array<uint8_t, PLDM_GET_COMMANDS_REQ_BYTES + sizeof(pldm_msg_hdr)>
        requestMsg{};
    pldm_msg* request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    request->body.payload = requestMsg.data() + sizeof(pldm_msg_hdr);
    request->body.payload_length = PLDM_GET_COMMANDS_REQ_BYTES;
    encode_get_commands_req(0, pldmType, version, request);
    mctp_message_tx(mctp[0], 0, requestMsg.data(),
                    PLDM_GET_COMMANDS_REQ_BYTES + sizeof(pldm_msg_hdr));

    mctp_serial_read(serial[1]);

    return EXIT_SUCCESS;
}
