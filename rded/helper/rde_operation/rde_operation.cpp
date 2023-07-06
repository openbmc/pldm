#include "libbej/bej_dictionary.h"
#include "libpldm/base.h"
#include "libpldm/pldm.h"
#include "libpldm/pldm_rde.h"
#include "libpldm/pldm_rde_requester.h"
#include "libpldm/utils.h"

#include "helper/discovery/base_discovery.hpp"
#include "helper/discovery/rde_discovery.hpp"
#include "helper/common.hpp"
#include "json.hpp"
#include "libbej/bej_decoder_json.hpp"
#include "libbej/bej_encoder_json.hpp"

#include <chrono>
#include <cstring>
#include <ctime>
#include <iostream>
#include <map>
#include <tuple>
#include <vector>

using json = nlohmann::json;

std::map<uint8_t, std::vector<uint8_t>*> responseRequestMap;

std::map<uint8_t, int> rdeOpCommandRequestSize = {
    {PLDM_RDE_OPERATION_INIT, 18},
    {PLDM_RDE_OPERATION_STATUS, 18},
    {PLDM_RDE_OPERATION_COMPLETE, 6},
    {PLDM_RDE_MULTIPART_RECEIVE, 7}};

int cleanupRequestId(uint8_t requestId)
{
    responseRequestMap.erase(requestId);
    return 0;
}

int getResponseForRequestId(uint8_t requestId, uint8_t** payload,
                                uint32_t* length)
{
    auto it = responseRequestMap.find(requestId);
    std::vector<uint8_t>* collectedPayload;
    if (it != responseRequestMap.end())
    {
        collectedPayload = it->second;
        *payload = &(*collectedPayload).front();
        *length = (*collectedPayload).size();
        return 0;
    }
    std::cerr << "Request id not found\n";
    return 1;
}

// int verifyChecksumForMultipartRecv(std::vector<uint8_t>* payload)
// {
//     uint32_t payloadLength = (*payload).size();
//     auto calculated_checksum = crc32(&(*payload).front(), payloadLength - 4);

//     uint8_t byte1 = (*payload).at(payloadLength - 4);
//     uint8_t byte2 = (*payload).at(payloadLength - 3);
//     uint8_t byte3 = (*payload).at(payloadLength - 2);
//     uint8_t byte4 = (*payload).at(payloadLength - 1);

//     // TODO: Check what happens with little endian and big endian context
//     uint32_t checksum_received = byte1 | byte2 << 8 | byte3 << 16 | byte4 << 24;

//     if (calculated_checksum == checksum_received)
//     {
//         std::cerr << " For RDE Op: Checksum Verified, Calculated Checksum: "
//                   << std::to_string(calculated_checksum)
//                   << " , Received Checksum: "
//                   << std::to_string(checksum_received) << "\n";
//         std::cerr << "====================================================\n";
//         return PLDM_RDE_REQUESTER_SUCCESS;
//     }
//     else
//     {
//         std::cerr
//             << "Checksum mismatch for request id/resource id in multipart receive\n";
//         return PLDM_RDE_REQUESTER_RECV_FAIL;
//     }
//     return PLDM_RDE_REQUESTER_RECV_FAIL;
// }

void readRequestCallback(struct pldm_rde_requester_manager* manager,
                           struct pldm_rde_requester_context* ctx,
                           /*payload_array*/ uint8_t** payload,
                           /*payloadLength*/ uint32_t payloadLength,
                           bool hasChecksum)
{
    IGNORE(manager);
    // std::cout << "Reaching callback\n";
    struct rde_operation* operationCtx =
        (struct rde_operation*)ctx->operation_ctx;
    uint8_t requestId = operationCtx->request_id;

    auto it = responseRequestMap.find(requestId);
    std::vector<uint8_t>* collectedPayload;
    if (it != responseRequestMap.end())
    {
        collectedPayload = it->second;
    }
    else
    {
        collectedPayload = new std::vector<uint8_t>();
        responseRequestMap.insert({requestId, collectedPayload});
    }
    std::cerr << "Payload Length inside creating map: "
              << std::to_string(payloadLength) << "\n";

    (*collectedPayload)
        .insert((*collectedPayload).end(), *payload,
                *payload + (payloadLength));
    if (hasChecksum)
    {
        int rc = verifyChecksumForMultipartRecv(collectedPayload);
        if (rc)
        {
            std::cerr << "Checksum failed for request id: " +
                             std::to_string(requestId);
        }
        else
        {
            std::cerr << "Checksum verifies for request id: "
                      << std::to_string(requestId) << "\n";
            // Remove the checksum from the payload
            (*collectedPayload)
                .erase((*collectedPayload).begin() +
                           ((*collectedPayload).size() - 4),
                       (*collectedPayload).end());
        }
    }
}

bool extractAction(std::string uri, std::string* resourceUri,
                    std::string* action)
{
    std::string actionString = "/Actions";
    size_t index;
    if ((index = uri.find(actionString)) != std::string::npos)
    {
        *resourceUri = uri.substr(0, index);
        size_t resourceIndex = (*resourceUri).rfind("/");
        if (resourceIndex != std::string::npos)
        {
            *action = uri.substr(index + 9, uri.length());
        }
        return 0;
    }
    return index;
}

// This is specific to Drive resource
int createOperationLocator(std::vector<uint8_t>* operationLocator,
                             uint8_t** schemaDict, uint16_t* finalOffset)
{
    int rc;
    // Pass uri from top down and get resource name

    std::vector<uint8_t> temp;
    uint16_t rootOffset = bejDictGetPropertyHeadOffset();
    uint16_t currentOffset;
    const struct BejDictionaryProperty* property = new BejDictionaryProperty();
    rc = bejDictGetPropertyByName(*schemaDict, rootOffset, "Drive", &property,
                                  &currentOffset);
    if (rc)
    {
        std::cerr << "Failed to find dictionary sequence for resource\n";
        return rc;
    }
    temp.push_back(property->sequenceNumber);

    rootOffset = property->childPointerOffset;
    rc = bejDictGetPropertyByName(*schemaDict, rootOffset, "Actions",
                                  &property, &currentOffset);
    if (rc)
    {
        std::cerr << "Failed to find dictionary sequence for resource\n";
        return rc;
    }
    temp.push_back(property->sequenceNumber);

    rootOffset = property->childPointerOffset;
    rc = bejDictGetPropertyByName(*schemaDict, rootOffset, "#Drive.Reset",
                                  &property, &currentOffset);
    if (rc)
    {
        std::cerr << "Failed to find dictionary sequence for resource\n";
        return rc;
    }
    temp.push_back(property->sequenceNumber);
    *finalOffset = currentOffset;

    for (auto sequenceNum : temp)
    {
        // Adding schema information
        // set lsb as 0 for major schema and 1 for annotation schema
        uint16_t byte = sequenceNum << 1;
        uint8_t byte1 = (uint8_t)(byte >> 8);
        uint8_t byte2 = (uint8_t)(byte & 0xFF);

        if (byte1 != 0)
        {
            operationLocator->emplace_back(0x2);
            operationLocator->emplace_back(byte1);
            operationLocator->emplace_back(byte2);
        }
        else
        {
            operationLocator->emplace_back(0x1);
            operationLocator->emplace_back(byte2);
        }
    }

    int size = operationLocator->size();
    operationLocator->insert(operationLocator->begin(), size);
    operationLocator->insert(operationLocator->begin(), 0x1);

    return 0;
}

int encodePayload(std::string requestPayload, std::string action,
                   uint16_t offset, uint8_t** schemaDict,
                   uint8_t** annotationDict,
                   std::vector<uint8_t>& encodedPayload)
{
    if (!requestPayload.empty())
    {
        BejDictionaries bejDictionaries;
        bejDictionaries.schemaDictionary = *schemaDict;
        bejDictionaries.annotationDictionary = *annotationDict;
        bejDictionaries.errorDictionary = NULL;

        struct RedfishPropertyParent root;
        std::string actionNode = "#" + action;
        std::cerr << "ACTION: " << actionNode << "\n";
        actionNode = "#Drive.Reset";
        bejTreeInitSet(&root, "#Drive.Reset");
        struct RedfishPropertyLeafEnum resetType;
        json request = json::parse(requestPayload);
        std::string cmd = request["ResetType"];
        bejTreeAddEnum(&root, &resetType, "ResetType", cmd.c_str());

        libbej::BejEncoderJson encoder;
        encoder.encode(&bejDictionaries, bejMajorSchemaClass, &root, offset);
        encodedPayload = encoder.getOutput();
    }
    return 0;
}

int encodePayloadForUpdateOp(std::string redfishReqPayload,
                                 uint8_t** schemaDict,
                                 uint8_t** annotationDict,
                                 std::vector<uint8_t>& encodedPayload)
{
    // Nikhil : Currently this encoding is only suitable to encode the patch
    // operation payload for rde pwm sensors (E.g. Payload {"Reading": 32.0}).
    // This needs to be made more generic to support patch operation on
    // different resource and their properites
    BejDictionaries bejDictionaries;
    bejDictionaries.schemaDictionary = *schemaDict;
    bejDictionaries.annotationDictionary = *annotationDict;
    bejDictionaries.errorDictionary = NULL;

    // Get rid of spaces from the string
    redfishReqPayload.erase(std::remove(redfishReqPayload.begin(),
                                          redfishReqPayload.end(), ' '),
                              redfishReqPayload.end());

    // Get rid of the outer curly braces from the patch data
    redfishReqPayload =
        redfishReqPayload.substr(1, redfishReqPayload.size() - 2);

    // Get rid of '"' from the property name
    redfishReqPayload.erase(std::remove(redfishReqPayload.begin(),
                                          redfishReqPayload.end(), '"'),
                              redfishReqPayload.end());

    size_t delimiterPos = redfishReqPayload.find(":");
    if (delimiterPos == std::string::npos)
    {
        std::cerr << "Delimiter ':' absent in the Redfish Req Data"
                  << std::endl;
        return -1;
    }

    std::string propertyName = redfishReqPayload.substr(0, delimiterPos);
    double propertyValue =
        std::stof(redfishReqPayload.substr(delimiterPos + 1));

    struct RedfishPropertyParent parent;
    bejTreeInitSet(&parent, nullptr);

    struct RedfishPropertyLeafReal child;
    bejTreeAddReal(&parent, &child, propertyName.c_str(), propertyValue);

    libbej::BejEncoderJson encoder;
    // Nikhil : Check return code
    encoder.encode(&bejDictionaries, bejMajorSchemaClass, &parent);
    encodedPayload = encoder.getOutput();
    return 0;
}

int processRdeOperationInit(int fd, uint8_t eid, int instanceId,
                               struct pldm_rde_requester_manager* manager,
                               struct pldm_rde_requester_context* ctx,
                               std::vector<uint8_t> requestMsg)
{
    auto start = std::chrono::system_clock::now();
    std::time_t start_time = std::chrono::system_clock::to_time_t(start);
    int rc;
    ctx->context_status = CONTEXT_BUSY;
    ctx->requester_status = PLDM_RDE_REQUESTER_WAITING_FOR_RESPONSE;

    std::cerr << "Sending on network: " << std::to_string(manager->net_id)
              << "\n";
    rc = pldm_send_at_network(eid, manager->net_id, fd, requestMsg.data(),
                              requestMsg.size());
    if (rc)
    {
        ctx->requester_status = PLDM_RDE_REQUESTER_REQUEST_FAILED;
        ctx->context_status = CONTEXT_FREE;
        return PLDM_RDE_REQUESTER_SEND_FAIL;
    }
    std::vector<uint8_t> response(sizeof(pldm_msg_hdr) +
                                  PLDM_MAX_REQUEST_BYTES);
    uint8_t* responseMsg = response.data();
    size_t responseMsgSize = sizeof(pldm_msg_hdr) + PLDM_MAX_REQUEST_BYTES;
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg);

    std::cerr << "Received response from network id: "
              << std::to_string(manager->net_id) << "\n";
    rc = pldm_recv_at_network(eid, fd, instanceId, &responseMsg,
                              &responseMsgSize, manager->net_id);
    if (rc)
    {
        ctx->requester_status = PLDM_RDE_REQUESTER_REQUEST_FAILED;
        ctx->context_status = CONTEXT_FREE;
        return PLDM_RDE_REQUESTER_RECV_FAIL;
    }

    auto end = std::chrono::system_clock::now();
    std::time_t end_time = std::chrono::system_clock::to_time_t(end);
    std::chrono::duration<double> elapsed_seconds = end - start;
    std::cout << "Started RDE Operation Init at: " << std::ctime(&start_time)
              << "\nRDE Operation Init at " << std::ctime(&end_time)
              << "\nTotal time: " << elapsed_seconds.count() << "s"
              << std::endl;

    rc = pldm_rde_push_read_operation_response(
        manager, ctx, responsePtr, responseMsgSize, &readRequestCallback);

    if (rc)
    {
        std::cerr << "Error in pushing response and updating context\n";
        return rc;
    }

    struct rde_operation* operation =
        (struct rde_operation*)ctx->operation_ctx;
    struct pldm_rde_varstring* resp_etag = operation->resp_etag;

    char* chars = new char[resp_etag->string_length_bytes + 1];
    memcpy(chars, resp_etag->string_data, resp_etag->string_length_bytes);
    return PLDM_RDE_REQUESTER_SUCCESS;
}

int processRdeOperationComplete(int fd, uint8_t eid, int instanceId,
                                   struct pldm_rde_requester_manager* manager,
                                   struct pldm_rde_requester_context* ctx,
                                   std::vector<uint8_t> requestMsg)
{
    int rc;
    ctx->context_status = CONTEXT_BUSY;
    ctx->requester_status = PLDM_RDE_REQUESTER_WAITING_FOR_RESPONSE;

    auto start = std::chrono::system_clock::now();
    std::time_t start_time = std::chrono::system_clock::to_time_t(start);
    std::cerr << "Sending complete op on network: "
              << std::to_string(manager->net_id) << "\n";

    rc = pldm_send_at_network(eid, manager->net_id, fd, requestMsg.data(),
                              requestMsg.size());

    if (rc)
    {
        ctx->requester_status = PLDM_RDE_REQUESTER_REQUEST_FAILED;
        ctx->context_status = CONTEXT_FREE;
        return PLDM_RDE_REQUESTER_SEND_FAIL;
    }

    std::vector<uint8_t> response(sizeof(pldm_msg_hdr) +
                                  PLDM_MAX_REQUEST_BYTES);
    uint8_t* responseMsg = response.data();
    size_t responseMsgSize = sizeof(pldm_msg_hdr) + PLDM_MAX_REQUEST_BYTES;
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg);
    std::cerr << "Receiving complete op on network: "
              << std::to_string(manager->net_id) << "\n";

    rc = pldm_recv_at_network(eid, fd, instanceId, &responseMsg,
                              &responseMsgSize, manager->net_id);
    if (rc)
    {
        ctx->requester_status = PLDM_RDE_REQUESTER_REQUEST_FAILED;
        ctx->context_status = CONTEXT_FREE;
        return PLDM_RDE_REQUESTER_RECV_FAIL;
    }

    auto end = std::chrono::system_clock::now();
    std::time_t end_time = std::chrono::system_clock::to_time_t(end);
    std::chrono::duration<double> elapsed_seconds = end - start;
    std::cout << "Started RDE Operation Complete at: "
              << std::ctime(&start_time) << "\nRDE Operation Complete at "
              << std::ctime(&end_time)
              << "\nTotal time for complete: " << elapsed_seconds.count() << "s"
              << std::endl;

    rc = pldm_rde_push_read_operation_response(manager, ctx, responsePtr,
                                               responseMsgSize, NULL);

    if (rc)
    {
        std::cerr << "Error in pushing response and updating context\n";
        return rc;
    }

    return PLDM_RDE_REQUESTER_SUCCESS;
}

int processRdeOpMultipartReceive(int fd, uint8_t eid, int instanceId,
                                     struct pldm_rde_requester_manager* manager,
                                     struct pldm_rde_requester_context* ctx,
                                     std::vector<uint8_t> requestMsg)
{
    auto start = std::chrono::system_clock::now();
    std::time_t start_time = std::chrono::system_clock::to_time_t(start);
    int rc;

    ctx->context_status = CONTEXT_BUSY;
    ctx->requester_status = PLDM_RDE_REQUESTER_WAITING_FOR_RESPONSE;

    std::cerr << "Sending multipart on network: "
              << std::to_string(manager->net_id) << "\n";
    rc = pldm_send_at_network(eid, manager->net_id, fd, requestMsg.data(),
                              requestMsg.size());

    if (rc)
    {
        ctx->requester_status = PLDM_RDE_REQUESTER_REQUEST_FAILED;
        ctx->context_status = CONTEXT_FREE;
        return PLDM_RDE_REQUESTER_SEND_FAIL;
    }

    std::vector<uint8_t> response(sizeof(pldm_msg_hdr) +
                                  PLDM_MAX_REQUEST_BYTES);
    uint8_t* responseMsg = response.data();
    size_t responseMsgSize = sizeof(pldm_msg_hdr) + PLDM_MAX_REQUEST_BYTES;
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg);

    std::cerr << "Receiving multipart on network: "
              << std::to_string(manager->net_id) << "\n";
    rc = pldm_recv_at_network(eid, fd, instanceId, &responseMsg,
                              &responseMsgSize, manager->net_id);
    if (rc)
    {
        ctx->requester_status = PLDM_RDE_REQUESTER_REQUEST_FAILED;
        ctx->context_status = CONTEXT_FREE;
        return PLDM_RDE_REQUESTER_RECV_FAIL;
    }

    auto end = std::chrono::system_clock::now();
    std::time_t end_time = std::chrono::system_clock::to_time_t(end);
    std::chrono::duration<double> elapsed_seconds = end - start;
    std::cout << "Started RDE Operation Init at: " << std::ctime(&start_time)
              << "\nRDE Operation Init at " << std::ctime(&end_time)
              << "\nTotal time: " << elapsed_seconds.count() << "s"
              << std::endl;

    rc = pldm_rde_push_read_operation_response(
        manager, ctx, responsePtr, responseMsgSize, &readRequestCallback);

    if (rc)
    {
        std::cerr << "Error in pushing response and updating context\n";
        return rc;
    }
    std::cerr << "Response pushed back\n";
    return 0;
}

int processNextRdeOperation(int fd, uint8_t eid, int instanceId,
                               struct pldm_rde_requester_manager* manager,
                               struct pldm_rde_requester_context* ctx,
                               std::vector<uint8_t> requestMsg)
{
    switch (ctx->next_command)
    {
        case PLDM_RDE_OPERATION_INIT:
            return processRdeOperationInit(fd, eid, instanceId, manager, ctx,
                                              requestMsg);

        case PLDM_RDE_OPERATION_COMPLETE:
            return processRdeOperationComplete(fd, eid, instanceId, manager,
                                                  ctx, requestMsg);
        case PLDM_RDE_MULTIPART_RECEIVE:
            return processRdeOpMultipartReceive(fd, eid, instanceId,
                                                    manager, ctx, requestMsg);

        default:
            return PLDM_RDE_REQUESTER_NOT_PLDM_RDE_MSG;
    }
    return 0;
}

int executeRdeOperation(int fd, int eid, int instanceId, std::string uri,
                          std::string udevid, uint16_t operationId,
                          uint8_t operationType, uint8_t requestId,
                          struct pldm_rde_requester_manager* manager,
                          struct pldm_rde_requester_context* ctx,
                          uint32_t resourceId, std::string reqJsonPayload)
{
    union pldm_rde_operation_flags* op_flags =
        (union pldm_rde_operation_flags*)malloc(
            sizeof(union pldm_rde_operation_flags));
    memset(op_flags, 0, sizeof(union pldm_rde_operation_flags));
    std::vector<uint8_t> operationLocator;

    std::vector<uint8_t> encodedPayload;
    int payloadLength;
    int operation_locator_length;
    int rc;

    // Enocde request payload for operations of type ACTION, CREATE and UPDATE
    if (operationType == 6)
    {
        op_flags->bits.contains_request_payload = 1;
        op_flags->bits.locator_valid = 1;

        std::string resource;
        std::string action;
        rc = extractAction(uri, &resource, &action);
        if (rc)
        {
            std::cerr << "No action URI found\n";
            return rc;
        }

        // Get RID for dictionary
        uint32_t ridSchema = resourceId >> 16 << 16;
        uint32_t schemaDictLen;
        uint8_t* schemaDict;
        rc = getDictionaryForRidDev(udevid, ridSchema, &schemaDict,
                                        &schemaDictLen);
        if (rc)
        {
            std::cerr << "Unable to get dictionary for resource";
            return rc;
        }

        uint32_t annotationDictLen;
        uint8_t* annotationDict;
        rc = getDictionaryForRidDev(udevid, 0xffffffff, &annotationDict,
                                        &annotationDictLen);
        if (rc)
        {
            std::cerr << "Unable to get annotation dictionary for resource";
            return rc;
        }

        // libbej::BejEncoderJson encoder;
        uint16_t offset;
        rc =
            createOperationLocator(&operationLocator, &schemaDict, &offset);

        rc = encodePayload(reqJsonPayload, action, offset, &schemaDict,
                            &annotationDict, encodedPayload);
    }
    else if (operationType == PLDM_RDE_OPERATION_UPDATE)
    {
        op_flags->bits.contains_request_payload = 1;

        int rc;
        uint32_t annotationDictLen;
        uint8_t* annotationDict;

        // get_dictioanry_for_rid from particular resource id
        rc = getDictionaryForRidDev(udevid, 0xffffffff, &annotationDict,
                                        &annotationDictLen);
        if (rc)
        {
            std::cerr << "Unable to fetch annotation dictionary\n";
            return rc;
        }
        uint32_t ridSchema = resourceId >> 16 << 16;
        uint32_t schemaDictLen;
        uint8_t* schemaDict;
        rc = getDictionaryForRidDev(udevid, ridSchema, &schemaDict,
                                        &schemaDictLen);
        if (rc)
        {
            std::cerr << "Unable to fetch schema dictionary\n";
            return rc;
        }

        rc = encodePayloadForUpdateOp(reqJsonPayload, &schemaDict,
                                          &annotationDict, encodedPayload);
        if (rc)
        {
            std::cerr << "Failed to bej encode payload for Rde Update operation"
                      << std::endl;
            return rc;
        }
    }

    rc = pldm_rde_init_rde_operation_context(
        ctx, requestId, resourceId, operationId, operationType, op_flags,
        /*send_transfer_handle*/ (uint32_t)0,
        /*operation_loc_length*/ operationLocator.size(),
        /*payloadLength*/ encodedPayload.size(),
        /*operation_locator_ptr*/ operationLocator.data(),
        /*requestPayload*/ encodedPayload.data());

    payloadLength = encodedPayload.size();
    operation_locator_length = operationLocator.size();
    if (rc)
    {
        std::cerr << "RDE Read operation init context failed\n";
        return rc;
    }
    int processCounter = 0;
    while (true)
    {
        if (ctx->requester_status == PLDM_RDE_REQUESTER_NO_PENDING_ACTION ||
            processCounter > 50)
        {
            break;
        }
        int requestBytes;

        if (rdeOpCommandRequestSize.find(ctx->next_command) !=
            rdeOpCommandRequestSize.end())
        {
            requestBytes = rdeOpCommandRequestSize[ctx->next_command];
        }
        else
        {
            requestBytes = PLDM_MAX_REQUEST_BYTES;
        }

        // If operation init request add payload
        // length to the request bytes
        if (ctx->next_command == PLDM_RDE_OPERATION_INIT)
        {
            requestBytes =
                requestBytes + payloadLength + operation_locator_length;
        }

        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) + requestBytes);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        std::cerr << "Getting next read operation...\n";

        int rc =
            pldm_rde_get_next_rde_operation(instanceId, manager, ctx, request);
        if (rc)
        {
            std::cerr << "No next operation\n";
            break;
        }

        rc = processNextRdeOperation(fd, eid, instanceId, manager, ctx,
                                        requestMsg);

        if (rc)
        {
            std::cerr << "Not a valid request to process\n";
            break;
        }
        processCounter++;
    }

    return 0;
}