#include "libpldm/base.h"
#include "libpldm/pldm.h"
#include "pldm_base_requester.h"
#include "libpldm/pldm_base_requester.h"

#include <cstring>
#include <iostream>
#include <vector>

std::map<uint8_t, int> command_request_size = {
    {PLDM_GET_TID, 0},
    {PLDM_GET_PLDM_TYPES, 0},
    {PLDM_GET_PLDM_VERSION, PLDM_GET_VERSION_REQ_BYTES},
    {PLDM_GET_PLDM_COMMANDS, PLDM_GET_COMMANDS_REQ_BYTES}};

pldm_base_requester_rc_t
    process_get_tid_request(int fd, uint8_t eid, int instanceId,
                            struct requester_base_context* ctx,
                            std::vector<uint8_t> requestMsg)
{
    int rc;
    ctx->command_status = COMMAND_WAITING;
    rc = pldm_send(eid, fd, requestMsg.data(), requestMsg.size());
    if (rc)
    {
        ctx->command_status = COMMAND_FAILED;
        return PLDM_BASE_REQUESTER_SEND_FAIL;
    }

    std::vector<uint8_t> response(
        sizeof(pldm_msg_hdr) + PLDM_GET_TID_RESP_BYTES, 0);
    uint8_t* responseMsg = response.data();
    size_t responseMsgSize = sizeof(pldm_msg_hdr) + PLDM_GET_TID_RESP_BYTES;
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg);

    rc = pldm_recv(eid, fd, instanceId, &responseMsg, &responseMsgSize);
    if (rc)
    {
        ctx->command_status = COMMAND_FAILED;
        return PLDM_BASE_REQUESTER_RECV_FAIL;
    }

    std::cout << "Pushing Response for GET_TID...\n";
    pldm_base_push_response(ctx, responsePtr, responseMsgSize);
    return PLDM_BASE_REQUESTER_SUCCESS;
}

pldm_base_requester_rc_t
    process_get_pldm_types_request(int fd, uint8_t eid, int instanceId,
                                   struct requester_base_context* ctx,
                                   std::vector<uint8_t> requestMsg)
{
    int rc;
    ctx->command_status = COMMAND_WAITING;
    rc = pldm_send(eid, fd, requestMsg.data(), requestMsg.size());
    if (rc)
    {
        ctx->command_status = COMMAND_FAILED;
        return PLDM_BASE_REQUESTER_SEND_FAIL;
    }

    std::vector<uint8_t> response(
        sizeof(pldm_msg_hdr) + PLDM_GET_TYPES_RESP_BYTES, 0);

    uint8_t* responseMsg = response.data();
    size_t responseMsgSize = sizeof(pldm_msg_hdr) + PLDM_GET_TYPES_RESP_BYTES;
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg);

    rc = pldm_recv(eid, fd, instanceId, &responseMsg, &responseMsgSize);
    if (rc)
    {
        ctx->command_status = COMMAND_FAILED;
        return PLDM_BASE_REQUESTER_RECV_FAIL;
    }
    std::cout << "Pushing Response for GET_PLDM_TYPES...\n";
    pldm_base_push_response(ctx, responsePtr, responseMsgSize);
    return PLDM_BASE_REQUESTER_SUCCESS;
}

pldm_base_requester_rc_t
    process_get_pldm_version_request(int fd, uint8_t eid, int instanceId,
                                     struct requester_base_context* ctx,
                                     std::vector<uint8_t> requestMsg)
{
    int rc;
    ctx->command_status = COMMAND_WAITING;
    rc = pldm_send(eid, fd, requestMsg.data(), requestMsg.size());
    if (rc)
    {
        return PLDM_BASE_REQUESTER_SEND_FAIL;
    }

    std::vector<uint8_t> response(
        sizeof(pldm_msg_hdr) + PLDM_GET_VERSION_RESP_BYTES, 0);
    uint8_t* responseMsg = response.data();
    size_t responseMsgSize = sizeof(pldm_msg_hdr) + PLDM_GET_VERSION_RESP_BYTES;
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg);
    rc = pldm_recv(eid, fd, instanceId, &responseMsg, &responseMsgSize);
    if (rc)
    {
        return PLDM_BASE_REQUESTER_RECV_FAIL;
    }

    std::cout << "Pushing Response for GET_PLDM_VERSION...\n";
    pldm_base_push_response(ctx, responsePtr, responseMsgSize);

    return PLDM_BASE_REQUESTER_SUCCESS;
}

pldm_base_requester_rc_t
    process_get_pldm_commands_request(int fd, uint8_t eid, int instanceId,
                                      struct requester_base_context* ctx,
                                      std::vector<uint8_t> requestMsg)
{
    int rc;
    ctx->command_status = COMMAND_WAITING;
    rc = pldm_send(eid, fd, requestMsg.data(), requestMsg.size());
    if (rc)
    {
        return PLDM_BASE_REQUESTER_SEND_FAIL;
    }

    std::vector<uint8_t> response(
        sizeof(pldm_msg_hdr) + PLDM_GET_COMMANDS_RESP_BYTES, 0);
    uint8_t* responseMsg = response.data();
    size_t responseMsgSize =
        sizeof(pldm_msg_hdr) + PLDM_GET_COMMANDS_RESP_BYTES;
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg);
    rc = pldm_recv(eid, fd, instanceId, &responseMsg, &responseMsgSize);
    if (rc)
    {
        return PLDM_BASE_REQUESTER_RECV_FAIL;
    }
    std::cout << "Pushing Response for GET_PLDM_COMMANDS...\n";
    pldm_base_push_response(ctx, responsePtr, responseMsgSize);

    return PLDM_BASE_REQUESTER_SUCCESS;
}

pldm_base_requester_rc_t
    process_next_request(int fd, uint8_t eid, int instanceId,
                         struct requester_base_context* ctx,
                         std::vector<uint8_t> requestMsg)
{
    pldm_base_requester_rc_t rc;
    switch (ctx->next_command)
    {
        case PLDM_GET_TID:
            rc = process_get_tid_request(fd, eid, instanceId, ctx, requestMsg);
            break;

        case PLDM_GET_PLDM_TYPES:
            rc = process_get_pldm_types_request(fd, eid, instanceId, ctx,
                                                requestMsg);
            break;
        case PLDM_GET_PLDM_VERSION:
            rc = process_get_pldm_version_request(fd, eid, instanceId, ctx,
                                                  requestMsg);
            break;
        case PLDM_GET_PLDM_COMMANDS:
            rc = process_get_pldm_commands_request(fd, eid, instanceId, ctx,
                                                   requestMsg);
            break;
        default:
            return PLDM_BASE_REQUESTER_NOT_PLDM_BASE_MSG;
    }
    if (rc)
    {
        return rc;
    }
    return PLDM_BASE_REQUESTER_SUCCESS;
}

// TODO: Remove the method below in final code
int printContext(struct requester_base_context* ctx)
{
    std::cerr << "====================================================\n";
    std::cerr << "***PLDM Context Begins***\n";
    std::cerr << "====================================================\n";
    std::cerr << "PLDM TID: " << (unsigned)ctx->tid << "\n";

    std::string types, commands, version;

    version += "Major: " + std::to_string(ctx->pldm_versions[0].major) +
               ", Minor: " + std::to_string(ctx->pldm_versions[0].minor) +
               ", Alpha: " + std::to_string(ctx->pldm_versions[0].alpha) +
               ", Update: " + std::to_string(ctx->pldm_versions[0].update);

    std::cerr << "PLDM Version For PLDM_BASE: " << version << "\n";

    for (auto bit : ctx->pldm_types)
    {
        types += std::to_string(bit) + ' ';
    }
    std::cerr << "PLDM TYPES: " << types << "\n";

    for (int i = 0; i < 32; i++)
    {
        commands += std::to_string(ctx->pldm_commands[i]) + ' ';
    }
    std::cerr << "PLDM Commands for PLDM Base: "
              << commands.substr(0, 20) + "..."
              << "\n";
    std::cerr << "====================================================\n";
    std::cerr << "***PLDM Context Ends***\n";
    std::cerr << "====================================================\n";
    return 0;
}

pldm_base_requester_rc_t umask_pldm_types(struct requester_base_context* ctx,
                     std::vector<uint8_t>* typesPresent)
{
    // While performing get Versions, take the pldmTypes
    // Run this function to get the list
    // Run the command for each value in the list
    int index, byte = 0;
    for (uint8_t pldmType : ctx->pldm_types)
    {
        index = 8 * byte;
        while (pldmType)
        {
            if (pldmType & 1)
            {
                // push the index to vector
                (*typesPresent).emplace_back(index);
            }
            pldmType = pldmType >> 1;
            index++;
        }
        byte++;
    }
    return PLDM_BASE_REQUESTER_SUCCESS;
}