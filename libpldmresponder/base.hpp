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
 *  @param[in] request - Request message
 *  @param[in] payloadLen - Request payload length
 *  @param[out] response - Response message written here
 */
void getPLDMTypes(const pldm_msg_t* request, size_t payloadLen,
                  pldm_msg_t* response);

/** @brief Handler for getPLDMCommands
 *
 *  @param[in] request - Request message
 *  @param[in] payloadLen - Request payload length
 *  @param[out] response - Response message written here
 */
void getPLDMCommands(const pldm_msg_t* request, size_t payloadLen,
                     pldm_msg_t* response);

/** @brief Handler for getPLDMCommands
 *
 *  @param[in] request - Request message
 *  @param[in] payloadLen - Request payload length
 *  @param[out] response - Response messsage written here
 */
void GetPLDMVersion(const pldm_msg_t* request, size_t payloadLen,
                    pldm_msg_t* response);

} // namespace responder
} // namespace pldm
