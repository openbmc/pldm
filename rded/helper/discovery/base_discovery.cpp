#include "libpldm/base.h"
#include "libpldm/pldm.h"
#include "libpldm/pldm_base_requester.h"

#include "helper/common.hpp"

#include <cstring>
#include <iostream>
#include <map>
#include <vector>

// TODO(@harshtya): Write test case for the functions once docker image is
// created with the latest code dependenncies

const bool DEBUG = false;
const int BASE_REQUEST_RETRIES = 50;

constexpr int DEBUG_PRINT_COMMAND_SIZE = 32;
constexpr uint8_t PLDM_RDE = 0x06;

std::map<uint8_t, int> baseCommandRequestSize = {
    {PLDM_GET_TID, 0},
    {PLDM_GET_PLDM_TYPES, 0},
    {PLDM_GET_PLDM_VERSION, PLDM_GET_VERSION_REQ_BYTES},
    {PLDM_GET_PLDM_COMMANDS, PLDM_GET_COMMANDS_REQ_BYTES}};

int processGetTidRequest(int fd, uint8_t eid, int instanceId,
                         struct requester_base_context* ctx,
                         const std::vector<uint8_t>& requestMsg)
{
    int rc;
    rc = pldm_send_at_network(eid, ctx->net_id, fd, requestMsg.data(),
                              requestMsg.size());
    if (rc)
    {
        ctx->requester_status = PLDM_BASE_REQUESTER_REQUEST_FAILED;
        return PLDM_BASE_REQUESTER_SEND_FAIL;
    }
    std::vector<uint8_t> response(
        sizeof(pldm_msg_hdr) + PLDM_GET_TID_RESP_BYTES, 0);
    uint8_t* responseMsg = response.data();
    size_t responseMsgSize = sizeof(pldm_msg_hdr) + PLDM_GET_TID_RESP_BYTES;
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg);
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

int processGetPldmTypesRequest(int fd, uint8_t eid, int instanceId,
                               struct requester_base_context* ctx,
                               const std::vector<uint8_t>& requestMsg)
{
    if (pldm_send_at_network(eid, ctx->net_id, fd, requestMsg.data(),
                             requestMsg.size()))
    {
        ctx->requester_status = PLDM_BASE_REQUESTER_REQUEST_FAILED;
        return PLDM_BASE_REQUESTER_SEND_FAIL;
    }
    std::vector<uint8_t> response(
        sizeof(pldm_msg_hdr) + PLDM_GET_TYPES_RESP_BYTES, 0);
    uint8_t* responseMsg = response.data();
    size_t responseMsgSize = response.size();
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg);
    if (pldm_recv_at_network(eid, fd, instanceId, &responseMsg,
                             &responseMsgSize, ctx->net_id))
    {
        ctx->requester_status = PLDM_BASE_REQUESTER_REQUEST_FAILED;
        return PLDM_BASE_REQUESTER_RECV_FAIL;
    }
    std::cerr << "Pushing Response for GET_PLDM_TYPES...\n";
    pldm_base_push_response(ctx, responsePtr, responseMsgSize);
    return PLDM_BASE_REQUESTER_SUCCESS;
}

int processGetPldmVersionRequest(int fd, uint8_t eid, int instanceId,
                                 struct requester_base_context* ctx,
                                 const std::vector<uint8_t>& requestMsg)
{
    if (pldm_send_at_network(eid, ctx->net_id, fd, requestMsg.data(),
                             requestMsg.size()))
    {
        ctx->requester_status = PLDM_BASE_REQUESTER_REQUEST_FAILED;
        return PLDM_BASE_REQUESTER_SEND_FAIL;
    }
    std::vector<uint8_t> response(
        sizeof(pldm_msg_hdr) + PLDM_GET_VERSION_RESP_BYTES, 0);
    uint8_t* responseMsg = response.data();
    size_t responseMsgSize = response.size();
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg);

    if (pldm_recv_at_network(eid, fd, instanceId, &responseMsg,
                             &responseMsgSize, ctx->net_id))
    {
        ctx->requester_status = PLDM_BASE_REQUESTER_REQUEST_FAILED;
        return PLDM_BASE_REQUESTER_RECV_FAIL;
    }
    std::cerr << "Pushing Response for GET_PLDM_VERSION...\n";
    pldm_base_push_response(ctx, responsePtr, responseMsgSize);
    return PLDM_BASE_REQUESTER_SUCCESS;
}

int processGetPldmCommandsRequest(int fd, uint8_t eid, int instanceId,
                                  struct requester_base_context* ctx,
                                  const std::vector<uint8_t>& requestMsg)
{
    if (pldm_send_at_network(eid, ctx->net_id, fd, requestMsg.data(),
                             requestMsg.size()))
    {
        ctx->requester_status = PLDM_BASE_REQUESTER_REQUEST_FAILED;
        return PLDM_BASE_REQUESTER_SEND_FAIL;
    }
    std::vector<uint8_t> response(
        sizeof(pldm_msg_hdr) + PLDM_GET_COMMANDS_RESP_BYTES, 0);
    uint8_t* responseMsg = response.data();
    size_t responseMsgSize = response.size();
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg);

    if (pldm_recv_at_network(eid, fd, instanceId, &responseMsg,
                             &responseMsgSize, ctx->net_id))
    {
        ctx->requester_status = PLDM_BASE_REQUESTER_REQUEST_FAILED;
        return PLDM_BASE_REQUESTER_RECV_FAIL;
    }
    std::cerr << "Pushing Response for GET_PLDM_COMMANDS...\n";
    pldm_base_push_response(ctx, responsePtr, responseMsgSize);
    return PLDM_BASE_REQUESTER_SUCCESS;
}

int processNextRequest(int fd, uint8_t eid, int instanceId,
                       struct requester_base_context* ctx,
                       const std::vector<uint8_t>& requestMsg)
{
    switch (ctx->next_command)
    {
        case PLDM_GET_TID:
            return processGetTidRequest(fd, eid, instanceId, ctx, requestMsg);
        case PLDM_GET_PLDM_TYPES:
            return processGetPldmTypesRequest(fd, eid, instanceId, ctx,
                                              requestMsg);
        case PLDM_GET_PLDM_VERSION:
            return processGetPldmVersionRequest(fd, eid, instanceId, ctx,
                                                requestMsg);
        case PLDM_GET_PLDM_COMMANDS:
            return processGetPldmCommandsRequest(fd, eid, instanceId, ctx,
                                                 requestMsg);
        default:
            return PLDM_BASE_REQUESTER_NOT_PLDM_BASE_MSG;
    }
    return PLDM_BASE_REQUESTER_SUCCESS;
}

// This function is called only if DEBUG flag is on
int printContext(struct requester_base_context* ctx)
{
    std::cerr << "====================================================\n";
    std::cerr << "***PLDM Context Begins***\n";
    std::cerr << "====================================================\n";
    std::cerr << "PLDM TID: " << (unsigned)ctx->tid << "\n";
    std::string types, commands_base, commands_rde, version_base, version_rde;
    version_base +=
        "Major: " + std::to_string(ctx->pldm_versions[PLDM_BASE].major) +
        ", Minor: " + std::to_string(ctx->pldm_versions[PLDM_BASE].minor) +
        ", Alpha: " + std::to_string(ctx->pldm_versions[PLDM_BASE].alpha) +
        ", Update: " + std::to_string(ctx->pldm_versions[PLDM_BASE].update);
    version_rde +=
        "Major: " + std::to_string(ctx->pldm_versions[PLDM_RDE].major) +
        ", Minor: " + std::to_string(ctx->pldm_versions[PLDM_RDE].minor) +
        ", Alpha: " + std::to_string(ctx->pldm_versions[PLDM_RDE].alpha) +
        ", Update: " + std::to_string(ctx->pldm_versions[PLDM_RDE].update);
    for (auto bit : ctx->pldm_types)
    {
        types += std::to_string(bit.byte) + ' ';
    }
    for (int i = 0; i < DEBUG_PRINT_COMMAND_SIZE; i++)
    {
        commands_base += std::to_string(ctx->pldm_commands[PLDM_BASE][i]) + ' ';
        commands_rde += std::to_string(ctx->pldm_commands[PLDM_RDE][i]) + ' ';
    }
    std::cerr << "Supported PLDM Types: " << types << "\n";
    std::cerr << "PLDM Version For PLDM_BASE: " << version_base << "\n";
    std::cerr << "PLDM Version For PLDM_RDE: " << version_rde << "\n";
    std::cerr << "PLDM Commands for PLDM Base: "
              << commands_base.substr(0, 20) + "..." // Prints partial results
              << "\n";
    std::cerr << "PLDM Commands for PLDM RDE: "
              << commands_rde.substr(0, 20) + "..." // Prints partial results
              << "\n";
    std::cerr << "====================================================\n";
    std::cerr << "***PLDM Context Ends***\n";
    std::cerr << "====================================================\n";
    return 0;
}

int performBaseDiscovery(std::string_view rdeDevice, int fd, int netId, int eid,
                         int instanceId)
{
    int rc = 0;
    struct requester_base_context* ctx = new requester_base_context();
    rc = pldm_base_init_context(ctx, rdeDevice.data(), netId);
    if (-1 == rc)
    {
        std::cerr
            << "Error in initializing RDE Requester Context, Return Code: "
            << rc << "\n";
        return rc;
    }
    std::cerr << "Triggering PLDM Base discovery...\n";
    rc = pldm_base_start_discovery(ctx);
    if (-1 == rc)
    {
        std::cerr << "Error in triggering PLDM_BASE, Return Code: " << rc
                  << "\n";
        return rc;
    }

    int processCounter = 0;

    while (true)
    {
        if (ctx->requester_status == PLDM_BASE_REQUESTER_NO_PENDING_ACTION)
        {
            break;
        }
        int requestBytes;
        if (baseCommandRequestSize.find(ctx->next_command) !=
            baseCommandRequestSize.end())
        {
            requestBytes = baseCommandRequestSize[ctx->next_command];
        }
        else
        {
            requestBytes = PLDM_MAX_REQUEST_BYTES;
        }
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) + requestBytes);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        std::cerr << "Getting next request...\n";
        rc = pldm_base_get_next_request(ctx, instanceId, request);
        if (rc)
        {
            std::cerr << "No more requests to process\n";
            break;
        }
        rc = processNextRequest(fd, eid, instanceId, ctx, requestMsg);
        if (rc)
        {
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

    if (DEBUG)
    {
        printContext(ctx);
    }
    std::cerr << "PLDM Base Discovery and context completed for " << rdeDevice
              << "\n";

    return rc;
}
