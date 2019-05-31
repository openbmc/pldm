#pragma once

#include <stdint.h>

#include <map>
#include <vector>

#include "libpldm/platform.h"
#include "libpldm/states.h"

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

/** @brief Handler for setStateEffecterStates
 *
 *  @param[in] request - Request message payload
 *  @param[in] payloadLength - Request payload length
 *  @param[out] Response - Response message written here
 */
Response setStateEffecterStates(const pldm_msg* request, size_t payloadLength);

/** @brief Function to actually set the effecter requested by host
 *  @param[in] effecterId - Effecter ID sent by the host to act on
 *  @param[in] stateField - The state field data for each of the states, equal
 *        to composite effecter count in number
 *  @param[out] - Success or failure in setting the states. Returns failure if
 *        atleast one state fails to be set
 */
int setStateEffecterStatesHandler(
    uint16_t effecterId,
    const std::vector<set_effecter_state_field>& stateField);

} // namespace responder
} // namespace pldm
