#pragma once

#include <stdint.h>

#include "libpldm/platform.h"

namespace pldm
{

namespace responder
{

namespace platform
{
/** @brief Register handlers for command from the platform spec
 */
void registerHandlers();
} // namespace platform

/** @brief Handler for setStateEffecterStates
 *
 *  @param[in] request - Request message payload
 *  @param[out] response - Response message written here
 */
void setStateEffecterStates(const pldm_msg_payload* request,
                            pldm_msg* response);

} // namespace responder

} // namespace pldm
