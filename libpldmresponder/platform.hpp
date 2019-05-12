#pragma once

#include <stdint.h>

#include <vector>

#include "libpldm/platform.h"

namespace pldm
{

using Response = std::vector<uint8_t>;

namespace responder
{

/** @brief Handler for GetPDR
 *
 *  @param[in] request - Request message payload
 *  @param[in] payloadLength - Request payload length
 *  @param[out] Response - Response message written here
 */
Response getPDR(const pldm_msg* request, size_t payloadLength);

} // namespace responder
} // namespace pldm
