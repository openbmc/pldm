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
                                  const size_t payload_length);

/** @brief Handler for getPLDMCommands
 *
 *  @param[in] request - Request message payload
 *  @param[in] payload_length - Request message payload length
 */
std::vector<uint8_t> getPLDMCommands(const pldm_msg* request,
                                     const size_t payload_length);

/** @brief Handler for getPLDMCommands
 *
 *  @param[in] request - Request message payload
 *  @param[in] payload_length - Request message payload length
 */
std::vector<uint8_t> getPLDMVersion(const pldm_msg* request,
                                    const size_t payload_length);

} // namespace responder
} // namespace pldm
