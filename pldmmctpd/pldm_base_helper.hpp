#include "libpldm/pldm_base_requester.h"

#include <iostream>
#include <map>
#include <vector>

#define PLDM_MAX_REQUEST_BYTES 255;

extern std::map<uint8_t, int> command_request_size;

/**
 * @brief Processes the next request based on the request type (attribute
 * `next_command`) in the requester context Sends the request, receives the
 * response and the calls the requester lib to push the response in the context
 *
 * @param[in] fd - Socket id
 *
 * @param[in] eid - Endpoint ID on which the request is being sent
 *
 * @param[in] ctx - Current requester context
 *
 * @param[in] requestMsg - Request Message Byte array
 *
 * @return pldm_base_requester_rc_t (errno may be set)
 */
pldm_base_requester_rc_t
    process_next_request(int fd, uint8_t eid, int instanceId,
                         struct requester_base_context* ctx,
                         std::vector<uint8_t> requestMsg);

/**
 * @brief Finds the bits set in bitfield8_t array returned in PLDM Types
 * response to get the corresponding PLDM Types supported
 *
 * @param[in] ctx - Requester Context that holds the PLDM Types bifield8_t array
 * (Follows the spec DSP0240_1.1.0 Table 12)
 *
 * @param[out] typesPresent - Vector containing PLDM Type hex codes for the
 * corresponding bits that were set in the PLDM Types attribute of the context
 *
 * @return pldm_base_requester_rc_t (errno may be set)
 */
pldm_base_requester_rc_t umask_pldm_types(struct requester_base_context* ctx,
                                          std::vector<uint8_t>* typesPresent);

// TODO: Remove in final code
int printContext(struct requester_base_context* ctx);