#pragma once

#include <stdint.h>

#include <vector>

#include "libpldm/base.h"

namespace pldm
{

using Type = uint8_t;

namespace responder
{

/** @brief Handler for getPLDMTypes
 *
 *  @param[in] request - Request message payload
 *  @param[out] response - Response message written here
 */
void getPLDMTypes(const pldm_msg* request, pldm_msg* response);

/** @brief Handler for getPLDMCommands
 *
 *  @param[in] request - Request message payload
 *  @param[out] response - Response message written here
 */
void getPLDMCommands(const pldm_msg* request, pldm_msg* response,
                     uint8_t payload_length);

/** @brief Handler for getPLDMCommands
 *
 *  @param[in] request - Request message payload
 *  @param[out] response - Response messsage written here
 */
void getPLDMVersion(const pldm_msg* request, pldm_msg* response,
                    uint8_t payload_length);

} // namespace responder
} // namespace pldm
