#include "helper/discovery/rde_discovery.hpp"

#include "libpldm/base.h"
#include "libpldm/pldm.h"
#include "libpldm/pldm_rde.h"
#include "libpldm/pldm_rde_requester.h"
#include "libpldm/utils.h"

#include "helper/common.hpp"

#include <cstring>
#include <iostream>
#include <map>
#include <tuple>
#include <vector>

std::vector<uint32_t> resourceIds;
std::map<std::string, int> managerCtxCounter;
std::map<std::string, struct pldm_rde_requester_manager*> managers;
struct pldm_rde_requester_context rdeContexts[256];
int rdeContextCounter = 0;
uint8_t numberOfResources = 9;
bool DEBUG = true;
std::map<uint8_t, int> rdeCommandRequestSize = {
    {PLDM_NEGOTIATE_REDFISH_PARAMETERS, 3},
    {PLDM_NEGOTIATE_MEDIUM_PARAMETERS, 4},
    {PLDM_GET_SCHEMA_DICTIONARY, 5},
    {PLDM_RDE_MULTIPART_RECEIVE, 7}};

int printRDENegotiateContext(struct pldm_rde_requester_manager* mgr);

// This map will store dictionaries for each of the RDE Devices separately
std::map<std::string, std::map<uint32_t, std::vector<uint8_t>*>*>
    dictionaryDeviceMap;
std::map<std::string, std::map<uint32_t, std::tuple<uint8_t, uint8_t>>*>
    dictionaryMetaDeviceMap;

// Callback function to allocate memory to contexts and keep them in an array
struct pldm_rde_requester_context*
    allocateMemoryToContexts(uint8_t numberOfContexts)
{
    int rc;
    int end = rdeContextCounter + numberOfContexts;
    for (rdeContextCounter = 0; rdeContextCounter < end; rdeContextCounter++)
    {
        struct pldm_rde_requester_context* currentCtx =
            (struct pldm_rde_requester_context*)malloc(
                sizeof(struct pldm_rde_requester_context));
        IGNORE(rc);
        rc = pldm_rde_create_context(currentCtx);
        if (rc)
        {
            return NULL;
        }
        rdeContexts[rdeContextCounter] = *currentCtx;
    }

    return &rdeContexts[0];
}

void freeMemory(void* context)
{
    free(context);
}

int initManager(std::string rdeDeviceId, int netId)
{
    int rc = -1;
    uint8_t mcConcurrency = uint8_t(3);
    uint32_t mcTransferSize = uint32_t(PLDM_MAX_REQUEST_BYTES);
    bitfield16_t* mcFeatures = (bitfield16_t*)malloc(sizeof(bitfield16_t));
    mcFeatures->value = (uint16_t)102; // Fixed for now
    std::cout << "Initializing RDE Manager...\n";

    resourceIds.emplace_back(0x00000000);
    resourceIds.emplace_back(0x00010000);
    resourceIds.emplace_back(0x00020000);
    resourceIds.emplace_back(0x00030000);
    resourceIds.emplace_back(0x00040000);
    resourceIds.emplace_back(0x00050000);
    resourceIds.emplace_back(0x00060000);
    resourceIds.emplace_back(0x00070000);
    resourceIds.emplace_back(0xffffffff);

    struct pldm_rde_requester_manager* manager =
        new pldm_rde_requester_manager();

    managerCtxCounter.insert({rdeDeviceId, rdeContextCounter});
    rc = pldm_rde_init_context(rdeDeviceId.c_str(), netId, manager,
                               mcConcurrency, mcTransferSize, mcFeatures,
                               numberOfResources, &resourceIds.front(),
                               allocateMemoryToContexts, freeMemory);

    if (rc)
    {
        std::cerr
            << "RDE Discovery failed due to context initialization error: "
            << rc << "\n";
        return rc;
    }

    managers.insert({rdeDeviceId, std::move(manager)});
    return PLDM_RDE_REQUESTER_SUCCESS;
}

int getRdeContextForRdeDevice(std::string rdeDevice)
{
    auto it = managerCtxCounter.find(rdeDevice);
    if (it != managerCtxCounter.end())
    {
        return it->second;
    }
    return -1;
}

int getManagerForRdeDevice(std::string rdeDevice,
                           struct pldm_rde_requester_manager** manager)
{
    auto it = managers.find(rdeDevice);
    if (it != managers.end())
    {
        *manager = it->second;
        return 0;
    }
    return -1;
}

int processNegotiateRedfishParametersRequest(
    int fd, uint8_t eid, int instanceId,
    struct pldm_rde_requester_manager* manager,
    struct pldm_rde_requester_context* ctx, std::vector<uint8_t> requestMsg)
{
    int rc;
    ctx->context_status = CONTEXT_BUSY;
    ctx->requester_status = PLDM_RDE_REQUESTER_WAITING_FOR_RESPONSE;
    rc = pldm_send_at_network(eid, manager->net_id, fd, requestMsg.data(),
                              requestMsg.size());
    if (rc)
    {
        ctx->requester_status = PLDM_RDE_REQUESTER_REQUEST_FAILED;
        ctx->context_status = CONTEXT_FREE;
        return PLDM_RDE_REQUESTER_SEND_FAIL;
    }
    std::vector<uint8_t> response(
        sizeof(pldm_msg_hdr) + RDE_NEGOTIATE_REDFISH_PARAMS_RESP_SIZE, 0);
    uint8_t* responseMsg = response.data();
    size_t responseMsgSize =
        sizeof(pldm_msg_hdr) + RDE_NEGOTIATE_REDFISH_PARAMS_RESP_SIZE;
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg);
    rc = pldm_recv_at_network(eid, fd, instanceId, &responseMsg,
                              &responseMsgSize, manager->net_id);
    if (rc)
    {
        ctx->requester_status = PLDM_RDE_REQUESTER_REQUEST_FAILED;
        return PLDM_RDE_REQUESTER_RECV_FAIL;
    }

    std::cerr << "Pushing Response for Negotiate Redfish Parameters...\n";

    rc = pldm_rde_discovery_push_response(manager, ctx, responsePtr,
                                          responseMsgSize);

    if (rc)
    {
        ctx->requester_status = PLDM_RDE_REQUESTER_REQUEST_FAILED;
        return rc;
    }
    return PLDM_BASE_REQUESTER_SUCCESS;
}

int processNegotiateMediumParametersRequest(
    int fd, uint8_t eid, int instanceId,
    struct pldm_rde_requester_manager* manager,
    struct pldm_rde_requester_context* ctx, std::vector<uint8_t> requestMsg)
{
    int rc;
    ctx->context_status = CONTEXT_BUSY;
    ctx->requester_status = PLDM_RDE_REQUESTER_WAITING_FOR_RESPONSE;
    rc = pldm_send_at_network(eid, manager->net_id, fd, requestMsg.data(),
                              requestMsg.size());
    if (rc)
    {
        ctx->requester_status = PLDM_RDE_REQUESTER_REQUEST_FAILED;
        ctx->context_status = CONTEXT_FREE;
        return PLDM_RDE_REQUESTER_SEND_FAIL;
    }

    std::vector<uint8_t> response(
        sizeof(pldm_msg_hdr) + RDE_NEGOTIATE_REDFISH_MEDIUM_PARAMS_RESP_SIZE,
        0);
    uint8_t* responseMsg = response.data();
    size_t responseMsgSize =
        sizeof(pldm_msg_hdr) + RDE_NEGOTIATE_REDFISH_MEDIUM_PARAMS_RESP_SIZE;
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg);

    rc = pldm_recv_at_network(eid, fd, instanceId, &responseMsg,
                              &responseMsgSize, manager->net_id);
    if (rc)
    {
        ctx->requester_status = PLDM_RDE_REQUESTER_REQUEST_FAILED;
        return PLDM_RDE_REQUESTER_RECV_FAIL;
    }

    std::cerr << "Pushing Response for Negotiate Medium Parameters...\n";

    rc = pldm_rde_discovery_push_response(manager, ctx, responsePtr,
                                          responseMsgSize);
    return PLDM_RDE_REQUESTER_SUCCESS;
}

int getDictionaryMap(
    std::string rdeDevice,
    std::map<uint32_t, std::vector<uint8_t>*>** dictionaryRidMap)
{
    std::cerr << "Searching dict for: " << rdeDevice
              << "with length: " << std::to_string(rdeDevice.length()) << "\n";
    auto it = dictionaryDeviceMap.find(rdeDevice);
    if (it != dictionaryDeviceMap.end())
    {
        *dictionaryRidMap = it->second;
        return 0;
    }
    return -1;
}

int verifyChecksumForMultipartRecv(std::vector<uint8_t>* payload)
{
    uint32_t payloadLength = (*payload).size();
    auto calculatedChecksum = crc32(&(*payload).front(), payloadLength - 4);

    uint8_t byte1 = (*payload).at(payloadLength - 4);
    uint8_t byte2 = (*payload).at(payloadLength - 3);
    uint8_t byte3 = (*payload).at(payloadLength - 2);
    uint8_t byte4 = (*payload).at(payloadLength - 1);

    // TODO: Check what happens with little endian and big endian context
    uint32_t checksumReceived = byte1 | byte2 << 8 | byte3 << 16 | byte4 << 24;

    if (calculatedChecksum == checksumReceived)
    {
        std::cerr << " For RDE Op: Checksum Verified, Calculated Checksum: "
                  << std::to_string(calculatedChecksum)
                  << " , Received Checksum: "
                  << std::to_string(checksumReceived) << "\n";
        std::cerr << "====================================================\n";
        return PLDM_RDE_REQUESTER_SUCCESS;
    }
    else
    {
        std::cerr
            << "Checksum mismatch for request id/resource id in multipart receive\n";
        return PLDM_RDE_REQUESTER_RECV_FAIL;
    }
    return PLDM_RDE_REQUESTER_RECV_FAIL;
}

void getDictMultipartResponseHandler(struct pldm_rde_requester_manager* manager,
                                     struct pldm_rde_requester_context* ctx,
                                     uint8_t** payload, uint32_t payloadLength,
                                     bool hasChecksum)
{
    uint8_t resourceIdIndex = ctx->current_pdr_resource->resource_id_index;
    uint32_t resourceId = manager->resource_ids[resourceIdIndex];

    if (DEBUG)
    {
        std::cerr << "Resource id pushing in dictionary...\t"
                  << std::to_string(
                         ctx->current_pdr_resource->resource_id_index)
                  << " and Resource id: " << std::to_string(resourceId)
                  << "\n";
    }
    std::string device_id = std::string(manager->device_name);
    std::map<uint32_t, std::vector<uint8_t>*>* dictionaryRidMap;
    int rc = getDictionaryMap(device_id, &dictionaryRidMap);
    if (rc)
    {
        std::cerr << "Unable to fetch dictionary from map while storing\n";
        return;
    }

    auto it = (*dictionaryRidMap).find(resourceId);
    std::vector<uint8_t>* dictionary;
    if (it != (*dictionaryRidMap).end())
    {
        dictionary = it->second;
    }
    else
    {
        dictionary = new std::vector<uint8_t>();
        (*dictionaryRidMap).insert({resourceId, dictionary});
    }
    if (DEBUG)
    {
        std::cerr << "Payload Length inside creating map for resource id << "
                  << std::to_string(
                         ctx->current_pdr_resource->resource_id_index)
                  << ": " << payloadLength << "\n";
    }

    (*dictionary)
        .insert((*dictionary).end(), *payload, *payload + (payloadLength));

    if (hasChecksum)
    {
        std::cerr << "Verifying checksum- last packet received\n";
        int rc = verifyChecksumForMultipartRecv(dictionary);
        if (rc)
        {
            std::cerr << "Checksum verification for dictionary failed\n";
        }
        else
        {
            std::cerr << "Checksum verified delteing last 4 payload\n";
            (*dictionary)
                .erase((*dictionary).begin() + ((*dictionary).size() - 4),
                       (*dictionary).end());
        }
    }
}

void responseHandlerCallback(struct pldm_rde_requester_manager* manager,
                             struct pldm_rde_requester_context* ctx,
                             uint8_t** payload, uint32_t payloadLength,
                             bool hasChecksum)
{
    switch (ctx->next_command)
    {
        case PLDM_RDE_MULTIPART_RECEIVE:
            getDictMultipartResponseHandler(manager, ctx, payload,
                                            payloadLength, hasChecksum);
            break;
        default:
            break; // No Op for other commands- Can add functionalities later
    }
}

int processGetNextDictionarySchema(int fd, uint8_t eid, int instanceId,
                                   struct pldm_rde_requester_manager* manager,
                                   struct pldm_rde_requester_context* ctx,
                                   std::vector<uint8_t> requestMsg)
{
    int rc;
    ctx->context_status = CONTEXT_BUSY;
    ctx->requester_status = PLDM_RDE_REQUESTER_WAITING_FOR_RESPONSE;
    rc = pldm_send_at_network(eid, manager->net_id, fd, requestMsg.data(),
                              requestMsg.size());
    if (rc)
    {
        ctx->requester_status = PLDM_RDE_REQUESTER_REQUEST_FAILED;
        ctx->context_status = CONTEXT_FREE;
        return PLDM_RDE_REQUESTER_SEND_FAIL;
    }
    std::vector<uint8_t> response(
        sizeof(pldm_msg_hdr) + RDE_GET_DICT_SCHEMA_RESP_SIZE, 0);
    uint8_t* responseMsg = response.data();
    size_t responseMsgSize =
        sizeof(pldm_msg_hdr) + RDE_GET_DICT_SCHEMA_RESP_SIZE;
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg);

    rc = pldm_recv_at_network(eid, fd, instanceId, &responseMsg,
                              &responseMsgSize, manager->net_id);
    if (rc)
    {
        ctx->requester_status = PLDM_RDE_REQUESTER_REQUEST_FAILED;
        return PLDM_RDE_REQUESTER_RECV_FAIL;
    }

    rc = pldm_rde_push_get_dictionary_response(
        manager, ctx, responsePtr, responseMsgSize, responseHandlerCallback);

    return PLDM_RDE_REQUESTER_SUCCESS;
}

int processRdeMultipartReceive(int fd, uint8_t eid, int instanceId,
                               struct pldm_rde_requester_manager* manager,
                               struct pldm_rde_requester_context* ctx,
                               std::vector<uint8_t> requestMsg)
{
    int rc;
    ctx->context_status = CONTEXT_BUSY;
    ctx->requester_status = PLDM_RDE_REQUESTER_WAITING_FOR_RESPONSE;
    rc = pldm_send_at_network(eid, manager->net_id, fd, requestMsg.data(),
                              requestMsg.size());
    if (rc)
    {
        ctx->requester_status = PLDM_RDE_REQUESTER_REQUEST_FAILED;
        ctx->context_status = CONTEXT_FREE;
        return PLDM_RDE_REQUESTER_SEND_FAIL;
    }
    std::vector<uint8_t> response(
        sizeof(pldm_msg_hdr) + manager->negotiated_transfer_size, 0);
    uint8_t* responseMsg = response.data();
    size_t responseMsgSize =
        sizeof(pldm_msg_hdr) + manager->negotiated_transfer_size;
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg);

    rc = pldm_recv_at_network(eid, fd, instanceId, &responseMsg,
                              &responseMsgSize, manager->net_id);
    if (rc)
    {
        ctx->requester_status = PLDM_RDE_REQUESTER_REQUEST_FAILED;
        return PLDM_RDE_REQUESTER_RECV_FAIL;
    }

    if (DEBUG)
    {
        auto resourceId =
            manager->resource_ids[ctx->current_pdr_resource->resource_id_index];
        auto transferHandleCurrent = ctx->current_pdr_resource->transfer_handle;
        std::cerr
            << "Pushing response for Get Multipart receive for resource id: "
            << std::to_string(resourceId)
            << " and transfer handle: " << std::to_string(transferHandleCurrent)
            << "...\n";
    }
    rc = pldm_rde_push_get_dictionary_response(
        manager, ctx, responsePtr, responseMsgSize, responseHandlerCallback);

    return PLDM_RDE_REQUESTER_SUCCESS;
}

int processNextRdeRequest(int fd, uint8_t eid, int instanceId,
                             struct pldm_rde_requester_manager* manager,
                             struct pldm_rde_requester_context* ctx,
                             std::vector<uint8_t> requestMsg)
{
    switch (ctx->next_command)
    {
        case PLDM_NEGOTIATE_REDFISH_PARAMETERS:
            return processNegotiateRedfishParametersRequest(
                fd, eid, instanceId, manager, ctx, requestMsg);

        case PLDM_NEGOTIATE_MEDIUM_PARAMETERS:
            return processNegotiateMediumParametersRequest(
                fd, eid, instanceId, manager, ctx, requestMsg);

        case PLDM_GET_SCHEMA_DICTIONARY:
            return processGetNextDictionarySchema(fd, eid, instanceId, manager,
                                                  ctx, requestMsg);

        case PLDM_RDE_MULTIPART_RECEIVE:
            return processRdeMultipartReceive(fd, eid, instanceId, manager, ctx,
                                              requestMsg);

        default:
            return PLDM_RDE_REQUESTER_NOT_PLDM_RDE_MSG;
    }
}

int performRdeDiscovery(std::string rdeDeviceId, int fd, int netId,
                        uint8_t destEid, uint8_t instanceId)
{
    int rc;
    rc = initManager(rdeDeviceId, netId);

    int rdeContextIndex = getRdeContextForRdeDevice(rdeDeviceId);
    if (rdeContextIndex == -1)
    {
        std::cerr
            << "Unable to provide base context- Error in RDE discovery\n ";
        return -1;
    }

    struct pldm_rde_requester_context baseContext =
        rdeContexts[rdeContextIndex];

    rc = pldm_rde_start_discovery(&baseContext);
    if (rc)
    {
        std::cerr << "RDE Discovery failed due to context busy with ec: " << rc
                  << "\n";
        return -1;
    }

    struct pldm_rde_requester_manager* manager;
    rc = getManagerForRdeDevice(rdeDeviceId, &manager);
    if (rc)
    {
        std::cerr << "Failure to get RDE Manager for rde device with ec: " << rc
                  << "\n";
        return -1;
    }

    int retryCounter = 0;
    while (true)
    {
        int requestBytes;
        if (baseContext.requester_status ==
            PLDM_RDE_REQUESTER_NO_PENDING_ACTION)
        {
            break;
        }
        if (rdeCommandRequestSize.find(baseContext.next_command) !=
            rdeCommandRequestSize.end())
        {
            requestBytes = rdeCommandRequestSize[baseContext.next_command];
        }
        else
        {
            requestBytes = PLDM_MAX_REQUEST_BYTES;
        }

        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) + requestBytes);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        std::cout << "Getting next RDE Discovery request...\n";

        rc = pldm_rde_get_next_discovery_command(instanceId, manager,
                                                 &baseContext, request);
        if (rc)
        {
            std::cerr << "Not a valid request to process\n";
            break;
        }

        rc = processNextRdeRequest(fd, destEid, 1, manager, &baseContext,
                                      requestMsg);

        if (rc)
        {
            std::cerr << "Unable to process next rde request: "
                      << std::to_string(rc) << "\n";
            break;
        }
        // std::cerr << "Request byte stream:" << rc << "\n";
        retryCounter++;
        if (retryCounter > 5)
        {
            break;
        }
    }

    if (DEBUG)
    {
        printRDENegotiateContext(manager);
    }
    return 0;
}

int performDictionaryDiscoveryForDevice(std::string rdeDeviceId, int fd,
                                        uint8_t destEid, uint8_t instanceId)
{
    int rc;
    std::map<uint32_t, std::vector<uint8_t>*>* dictionaryRidMap =
        new std::map<uint32_t, std::vector<uint8_t>*>();

    std::map<uint32_t, std::tuple<uint8_t, uint8_t>>* dictionaryMetaRidMap =
        new std::map<uint32_t, std::tuple<uint8_t, uint8_t>>();

    dictionaryDeviceMap.insert({rdeDeviceId, dictionaryRidMap});

    // For holding schema initial transfer flags - not being used right now
    dictionaryMetaDeviceMap.insert({rdeDeviceId, dictionaryMetaRidMap});

    int rdeContextIndex = getRdeContextForRdeDevice(rdeDeviceId);
    if (rdeContextIndex == -1)
    {
        std::cerr << "Unable to provide base context- Error in RDE discovery\n";
        return -1;
    }

    struct pldm_rde_requester_context baseContext =
        rdeContexts[rdeContextIndex];
    rc = pldm_rde_init_get_dictionary_schema(&baseContext);
    if (rc)
    {
        std::cerr << "PLDM RDE Init dictionary failed with ec: " << rc << "\n";
        return rc;
    }
    if (DEBUG)
    {
        std::cerr << "Getting dictionary for context resource id : "
                  << std::to_string(
                         (baseContext.current_pdr_resource)->resource_id_index)
                  << "\n";
    }

    struct pldm_rde_requester_manager* manager;
    rc = getManagerForRdeDevice(rdeDeviceId, &manager);
    if (rc)
    {
        std::cerr << "Failure to get RDE Manager for rde device with ec: " << rc
                  << "\n";
        return -1;
    }

    int retryCounter = 0;
    while (retryCounter < MAX_RETRIES_FOR_REQUEST)
    {
        int requestBytes;
        if (baseContext.requester_status ==
            PLDM_RDE_REQUESTER_NO_PENDING_ACTION)
        {
            break;
        }
        if (rdeCommandRequestSize.find(baseContext.next_command) !=
            rdeCommandRequestSize.end())
        {
            requestBytes = rdeCommandRequestSize[baseContext.next_command];
        }
        else
        {
            requestBytes = PLDM_MAX_REQUEST_BYTES;
        }
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) + requestBytes);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        std::cerr << "Getting next get dictionary request...\n";

        rc = pldm_rde_get_next_dictionary_schema_command(
            /*instanceId*/ instanceId, manager, &baseContext, request);

        if (rc)
        {
            std::cerr << "Not a valid request to process\n";
            break;
        }

        rc = processNextRdeRequest(fd, destEid, instanceId, manager,
                                      &baseContext, requestMsg);

        if (rc)
        {
            std::cerr << "Unable to process next rde request: "
                      << std::to_string(rc) << "\n";
            break;
        }
        retryCounter++;
    }

    return 0;
}

int getRdeFreeContextForRdeDevice(
    std::string rdeDevice, struct pldm_rde_requester_context* baseContext)
{
    auto it = managerCtxCounter.find(rdeDevice);
    if (it != managerCtxCounter.end())
    {
        *baseContext = rdeContexts[it->second];
        if (baseContext->context_status != CONTEXT_FREE)
        {
            return RDE_CONTEXT_NOT_FREE;
        }
        return RDE_CONTEXT_FREE;
    }
    return RDE_NO_CONTEXT_FOUND;
}

int getDictionaryForRidDev(std::string rdeDeviceId, uint32_t resourceId,
                               uint8_t** dictElem, uint32_t* dictLength) {
    std::map<uint32_t, std::vector<uint8_t>*>* dictionaryRidMap;
    int rc = getDictionaryMap(rdeDeviceId, &dictionaryRidMap);
    if (rc)
    {
        std::cerr << "Unable to fetch dictionary from map while storing\n";
        return -1;
    }

    auto it = (*dictionaryRidMap).find(resourceId);
    std::vector<uint8_t>* dictionary;
    if (it != (*dictionaryRidMap).end())
    {
        // std::cerr << "Got dictionary \n";
        dictionary = it->second;
        // std::cerr << "Dictionary Length: " << (*dictionary).size() << "\n";
        *dictElem = &(*dictionary).front();
        *dictLength = (*dictionary).size();
        return PLDM_RDE_REQUESTER_SUCCESS;
    }
    return PLDM_RDE_NO_PDR_RESOURCES_FOUND;
}

int printRDENegotiateContext(struct pldm_rde_requester_manager* mgr)
{
    std::cerr << "====================================================\n";
    std::cerr << "***RDE Negotiate/Medium Parameters Begins***\n";
    std::cerr << "====================================================\n";

    struct pldm_rde_device_info* deviceInfo = mgr->device;

    std::cerr << "MC Concurrency: " << std::to_string(mgr->mc_concurrency)
              << "\n";
    std::cerr << "MC Transfer Size: " << std::to_string(mgr->mc_transfer_size)
              << "\n";

    std::cerr << "miniBMC Concurrency: "
              << std::to_string(deviceInfo->device_concurrency) << "\n";
    std::cerr << "miniBMC Device Capabilities Flag: "
              << std::to_string(deviceInfo->device_capabilities_flag.byte)
              << "\n";
    std::cerr << "miniBMC Device Feature Support: "
              << std::to_string(deviceInfo->device_feature_support.value)
              << "\n";
    std::cerr << "miniBMC Device Configuration Signature: "
              << std::to_string(deviceInfo->device_configuration_signature)
              << "\n";
    std::cerr << "miniBMC Device Provider Name (Eid): "
              << std::to_string(
                     deviceInfo->device_provider_name.string_length_bytes)
              << "\n";

    std::cerr << "Negotiated Transfer Size (Medium Parameters): "
              << std::to_string(mgr->negotiated_transfer_size) << "\n";

    std::cerr << "====================================================\n";
    std::cerr << "***RDE Negotiate/Medium Parameters Ends***\n";
    std::cerr << "====================================================\n";
    return 0;
}