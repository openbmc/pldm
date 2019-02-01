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
void getPLDMTypes(const pldm_msg_payload_t* request, pldm_msg_t* response);

/** @brief Handler for getPLDMCommands
 *
 *  @param[in] request - Request message payload
 *  @param[out] response - Response message written here
 */
void getPLDMCommands(const pldm_msg_payload_t* request, pldm_msg_t* response);

} // namespace responder
} // namespace pldm
