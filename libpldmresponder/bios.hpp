#pragma once

#include <stdint.h>

#include "libpldm/bios.h"

namespace pldm
{

namespace responder
{
using EpochTimeUS = uint64_t;

/** @brief Handler for GetDateTime
 *
 *  @param[in] request - Request message payload
 *  @param[out] response - Response message written here
 */
void getDateTime(const pldm_msg_payload* request, pldm_msg* response);

/** @brief Format time to sec:min:hr:day:month:year format
 *
 *  @param[in] time - Time got from epoch time in seconds
 *  @param[out] seconds - number of seconds
 *  @param[out] minutes - number of minutes
 *  @param[out] hours - number of hours
 *  @param[out] month - month number
 *  @param[out] day - day of the month
 *  @param[out] year - year number
 */
void epochToBCDTime(uint64_t timeSec, uint8_t& seconds, uint8_t& minutes,
                    uint8_t& hours, uint8_t& day, uint8_t& month,
                    uint16_t& year);

} // namespace responder
} // namespace pldm
