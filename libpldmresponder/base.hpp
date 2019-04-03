#pragma once

#include <stdint.h>

#include <vector>

#include "libpldm/base.h"

namespace pldm
{

using Type = uint8_t;

using Response = std::vector<uint8_t>;

namespace responder
{

/** @brief Handler for getPLDMTypes
 *
 *  @param[in] request - Request message payload
 *  @param[in] payload_length - Request message payload length
 *  @param[return] Response - PLDM Response message
 */
Response getPLDMTypes(const pldm_msg* request, size_t payloadLength);

/** @brief Handler for getPLDMCommands
 *
 *  @param[in] request - Request message payload
 *  @param[in] payload_length - Request message payload length
 *  @param[return] Response - PLDM Response message
 */
Response getPLDMCommands(const pldm_msg* request, size_t payloadLength);

/** @brief Handler for getPLDMCommands
 *
 *  @param[in] request - Request message payload
 *  @param[in] payload_length - Request message payload length
 *  @param[return] Response - PLDM Response message
 */
Response getPLDMVersion(const pldm_msg* request, size_t payloadLength);
} // namespace responder
} // namespace pldm
