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
 *  @param[in] payload_length - Request message payload length
 */
std::vector<uint8_t> getPLDMTypes(const pldm_msg* request,
                                  size_t payloadLength);

/** @brief Handler for getPLDMCommands
 *
 *  @param[in] request - Request message payload
 *  @param[in] payload_length - Request message payload length
 */
std::vector<uint8_t> getPLDMCommands(const pldm_msg* request,
                                     size_t payloadLength);

/** @brief Handler for getPLDMCommands
 *
 *  @param[in] request - Request message payload
 *  @param[in] payload_length - Request message payload length
 */
void getPLDMVersion(const pldm_msg* request, size_t payloadLength,
                    std::vector<uint8_t>& response);

} // namespace responder
} // namespace pldm
