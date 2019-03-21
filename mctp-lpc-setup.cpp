#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <array>
#include <string.h>

#include "libmctp/libmctp.h"
#include "libmctp/libmctp-astlpc.h"
#include "mctp-lpc-setup.hpp"
#include "libpldm/base.h"
#include "registration.hpp"

namespace mctp_lpc
{

static const mctp_eid_t local_eid = 8;
static const mctp_eid_t remote_eid = 9;

static const uint8_t echo_req = 1;
static const uint8_t echo_resp = 2;

struct ctx {
        struct mctp     *mctp;
};

struct mctp_binding_astlpc *astlpc;
struct mctp *mctp;
struct ctx *ctx, _ctx;

static void tx_message(struct ctx *ctx, mctp_eid_t eid, void *msg, size_t len)
{
        uint8_t type;

        type = len > 0 ? *(uint8_t *)(msg) : 0x00;

        fprintf(stderr, "TX: dest EID 0x%02x: %zd bytes, first byte [0x%02x]\n",
                        eid, len, type);
        mctp_message_tx(ctx->mctp, eid, msg, len);
}

static void rx_message(uint8_t eid, void *data, void *msg, size_t len)
{
        struct ctx *ctx = (struct ctx *)data;
        uint8_t type;

        type = len > 0 ? *(uint8_t *)(msg) : 0x00;

        fprintf(stderr, "RX: src EID 0x%02x: %zd bytes, first byte [0x%02x]\n",
                        eid, len, type);
for(size_t i = 0; i < len; ++i)
    fprintf(stderr, "%.2x ", *((uint8_t*)msg + i));
fprintf(stderr, "\n");

        pldm_header_info hdrFields{};
        pldm_msg_hdr* hdr = reinterpret_cast<pldm_msg_hdr*>(static_cast<uint8_t*>(msg) + sizeof(uint8_t));
        if (PLDM_SUCCESS != unpack_pldm_header(hdr, &hdrFields))
        {
            fprintf(stderr, "Invalid PLDM request");
            return;
        }
        pldm_msg_payload body{};
        body.payload = static_cast<uint8_t*>(msg) + sizeof(uint8_t) + sizeof(pldm_msg_hdr);
        body.payload_length = len - sizeof(pldm_msg_hdr) - sizeof(uint8_t);

        std::array<uint8_t, PLDM_GET_COMMANDS_RESP_BYTES + sizeof(pldm_msg_hdr)>
            responseMsg{};
        pldm_msg response{};
        response.body.payload = responseMsg.data() + sizeof(pldm_msg_hdr);
        response.body.payload_length = PLDM_GET_COMMANDS_RESP_BYTES;

        pldm::responder::invokeHandler(hdrFields.pldm_type, hdrFields.command,
                                       &body, &response);
        memcpy(responseMsg.data(), &response, sizeof(pldm_msg_hdr));
for(size_t i = 0; i < PLDM_GET_COMMANDS_RESP_BYTES + sizeof(pldm_msg_hdr); ++i)
    fprintf(stderr, "%.2x ", *(responseMsg.data() + i));
fprintf(stderr, "\n");
        tx_message(ctx, eid, responseMsg.data(), PLDM_GET_COMMANDS_RESP_BYTES + sizeof(pldm_msg_hdr));
}

void setup()
{
        int rc;

        mctp = mctp_init();
        assert(mctp);

        astlpc = mctp_astlpc_init();
        assert(astlpc);

        mctp_astlpc_register_bus(astlpc, mctp, local_eid);

        ctx = &_ctx;
        ctx->mctp = mctp;

        mctp_set_rx_all(mctp, rx_message, ctx);

        for (;;) {

                (void)remote_eid;
                rc = mctp_astlpc_poll(astlpc);
                if (rc)
                        break;

       }
}

}
