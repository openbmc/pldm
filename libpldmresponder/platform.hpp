#pragma once

#include <stdint.h>

#include "libpldm/platform.h"

namespace pldm
{

namespace responder
{

/** @brief Handler for GetPDR
 *
 *  @param[in] request - Request message payload
 *  @param[out] response - Response message written here
 */
void getPDR(const pldm_msg_payload* request, pldm_msg* response);

} // namespace responder
} // namespace pldm
