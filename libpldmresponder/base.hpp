#pragma once

#include "registration.hpp"

#include <stdint.h>

#include <vector>

#include "libpldm/base.h"

namespace pldm
{

using Type = uint8_t;

namespace responder
{

namespace base
{
/** @brief Register handlers for command from the base spec
 */
void registerHandlers();
} // namespace base

/** @brief Handler for getPLDMTypes
 *
 *  @param[in] request - Request message
 *  @param[in] payload_length - Request message payload length
 *  @param[return] Response - PLDM Response message
 */
Response getPLDMTypes(const Interfaces& intfs, const Request& request,
                      size_t payloadLength);

/** @brief Handler for getPLDMCommands
 *
 *  @param[in] request - Request message
 *  @param[in] payload_length - Request message payload length
 *  @param[return] Response - PLDM Response message
 */
Response getPLDMCommands(const Interfaces& intfs, const Request& request,
                         size_t payloadLength);

/** @brief Handler for getPLDMCommands
 *
 *  @param[in] request - Request message
 *  @param[in] payload_length - Request message payload length
 *  @param[return] Response - PLDM Response message
 */
Response getPLDMVersion(const Interfaces& intfs, const Request& request,
                        size_t payloadLength);
} // namespace responder
} // namespace pldm
