#include "libpldm/base.h"
#include "libpldm/pldm.h"
#include "libpldm/pldm_base_requester.h"

#include "helper/common.hpp"

#include <cstring>
#include <iostream>
#include <vector>
#include <map>

// TODO(@harshtya): Remove when the all base commands supported
#define PLDM_NEXT_CMD_NOT_SUPPORTED 2

const bool DEBUG = false;
const int BASE_REQUEST_RETRIES = 50;

std::map<uint8_t, int> command_request_size = {
    {PLDM_GET_TID, 0},
    {PLDM_GET_PLDM_TYPES, 0},
    {PLDM_GET_PLDM_VERSION, PLDM_GET_VERSION_REQ_BYTES},
    {PLDM_GET_PLDM_COMMANDS, PLDM_GET_COMMANDS_REQ_BYTES}};

int process_get_tid_request(int fd, uint8_t eid, int instanceId,
                            struct requester_base_context* ctx,
                            std::vector<uint8_t> requestMsg)
{
    int rc;
    // ctx->command_status = COMMAND_WAITING;
    // rc = pldm_send(eid, fd, requestMsg.data(), requestMsg.size());
    rc = pldm_send_at_network(eid, ctx->net_id, fd, requestMsg.data(),
                              requestMsg.size());
    if (rc)
    {
        // ctx->command_status = COMMAND_FAILED;
        ctx->requester_status = PLDM_BASE_REQUESTER_REQUEST_FAILED;
        return PLDM_BASE_REQUESTER_SEND_FAIL;
    }
    std::vector<uint8_t> response(
        sizeof(pldm_msg_hdr) + PLDM_GET_TID_RESP_BYTES, 0);
    uint8_t* responseMsg = response.data();
    size_t responseMsgSize = sizeof(pldm_msg_hdr) + PLDM_GET_TID_RESP_BYTES;
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg);
    // rc = pldm_recv(eid, fd, instanceId, &responseMsg, &responseMsgSize);
    rc = pldm_recv_at_network(eid, fd, instanceId, &responseMsg,
                              &responseMsgSize, ctx->net_id);
    if (rc)
    {
        ctx->requester_status = PLDM_BASE_REQUESTER_REQUEST_FAILED;
        return PLDM_BASE_REQUESTER_RECV_FAIL;
    }
    std::cerr << "Pushing Response for GET_TID...\n";
    pldm_base_push_response(ctx, responsePtr, responseMsgSize);
    return PLDM_BASE_REQUESTER_SUCCESS;
}

int process_next_request(int fd, uint8_t eid, int instanceId,
                         struct requester_base_context* ctx,
                         std::vector<uint8_t> requestMsg)
{
    switch (ctx->next_command)
    {
        case PLDM_GET_TID:
            return process_get_tid_request(fd, eid, instanceId, ctx,
                                           requestMsg);
        case PLDM_GET_PLDM_TYPES:
            return PLDM_NEXT_CMD_NOT_SUPPORTED;
        case PLDM_GET_PLDM_VERSION:
            return PLDM_NEXT_CMD_NOT_SUPPORTED;
        case PLDM_GET_PLDM_COMMANDS:
            return PLDM_NEXT_CMD_NOT_SUPPORTED;
        default:
            return PLDM_BASE_REQUESTER_NOT_PLDM_BASE_MSG;
    }
    return PLDM_BASE_REQUESTER_SUCCESS;
}

int perform_base_discovery(std::string rde_device, int fd, int net_id, int eid,
                           int instance_id)
{
    int rc = 0;
    struct requester_base_context* ctx = new requester_base_context();
    rc = pldm_base_init_context(ctx, rde_device.c_str(), net_id);
    if (-1 == rc)
    {
        std::cerr
            << "Error in initializing RDE Requester Context, Return Code: "
            << rc << "\n";
        return -1;
    }
    std::cerr << "Triggering PLDM Base discovery...\n";
    rc = pldm_base_start_discovery(ctx);
    if (-1 == rc)
    {
        std::cerr << "Error in triggering PLDM_BASE, Return Code: " << rc
                  << "\n";
        return -1;
    }

    int processCounter = 0;

    while (true)
    {
        if (ctx->requester_status == PLDM_BASE_REQUESTER_NO_PENDING_ACTION)
        {
            break;
        }
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
        rc = pldm_base_get_next_request(ctx, instance_id, request);
        if (rc)
        {
            std::cerr << "No more requests to process\n";
            break;
        }
        rc = process_next_request(fd, eid, instance_id, ctx, requestMsg);
        if (rc)
        {
            // TODO(@harshtya): Remove when all PLDM Base commands supported
            if (rc == PLDM_NEXT_CMD_NOT_SUPPORTED)
            {
                std::cerr << "Supported PLDM Base discovery commands completed!"
                          << "Gracefully exiting..\n";
                break;
            }
            std::cerr << "Failure in processing request with error code:"
                      << std::to_string(rc) << "\n";
            break;
        }
        if (processCounter > BASE_REQUEST_RETRIES)
        {
            std::cerr
                << "MCTP setup error, no base discovery request is succeding\n";
            break;
        }
        processCounter++;
    }
    return rc;
}
