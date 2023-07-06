#include "libpldm/pldm_base_requester.h"
#include "libpldm/pldm_rde_requester.h"

#include "helper/common.hpp"

#include <iostream>
#include <map>
#include <vector>

extern std::map<uint8_t, int> rdeOpCommandRequestSize;

int processNextRdeOperation(int fd, uint8_t eid, int instanceId,
                               struct pldm_rde_requester_manager* manager,
                               struct pldm_rde_requester_context* ctx,
                               std::vector<uint8_t> requestMsg);

int executeRdeOperation(int fd, int eid, int instanceId, std::string uri,
                          std::string udevid, uint16_t operationId,
                          uint8_t operationType, uint8_t request_id,
                          struct pldm_rde_requester_manager* manager,
                          struct pldm_rde_requester_context* ctx,
                          uint32_t resourceId, std::string jsonPayload);

int getResponseForRequestId(uint8_t requestId, uint8_t** payload,
                                uint32_t* length);

int cleanupRequestId(uint8_t requestId);