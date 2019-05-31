#pragma once

#include <stdint.h>

#include <map>

#include "libpldm/platform.h"
#include "libpldm/states.h"

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

/** @brief Handler for setStateEffecterStates
 *
 *  @param[in] request - Request message payload
 *  @param[out] response - Response message written here
 */
void setStateEffecterStates(const pldm_msg_payload* request,
                            pldm_msg* response);

/** @brief Function to actually set the effecter requested by host
 *  @param[in] effecterId - Effecter ID sent by the host to act on
 *  @param[in] compEffecterCnt - Composite effecter count for the effecter
 *  @param[in] stateField - The state field data for each of the states, equal
 *        to compEffecterCnt in number
 *  @param[out] - Success or failure in setting the states. Returns failure if
 *        atleast one state fails to be set
 */

int setBmcEffecter(uint16_t& effecterId, uint8_t& compEffecterCnt,
                   set_effecter_state_field* stateField);

} // namespace responder
} // namespace pldm
