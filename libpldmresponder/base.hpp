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
void getPLDMTypes(const pldm_msg_payload* request, pldm_msg* response);

/** @brief Handler for getPLDMCommands
 *
 *  @param[in] request - Request message payload
 *  @param[out] response - Response message written here
 */
void getPLDMCommands(const pldm_msg_payload* request, pldm_msg* response);

} // namespace responder
} // namespace pldm
