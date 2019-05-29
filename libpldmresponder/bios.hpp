#pragma once

#include <stdint.h>

#include "libpldm/bios.h"

namespace pldm
{

namespace responder
{

namespace bios
{
/** @brief Register handlers for command from the platform spec
 */
void registerHandlers();
} // namespace bios

/** @brief Handler for GetDateTime
 *
 *  @param[in] request - Request message payload
 *  @param[out] response - Response message written here
 */
void getDateTime(const pldm_msg_payload* request, pldm_msg* response);

namespace utils
{

/** @brief Convert epoch time to BCD time
 *
 *  @param[in] timeSec - Time got from epoch time in seconds
 *  @param[out] seconds - number of seconds in BCD
 *  @param[out] minutes - number of minutes in BCD
 *  @param[out] hours - number of hours in BCD
 *  @param[out] day - day of the month in BCD
 *  @param[out] month - month number in BCD
 *  @param[out] year - year number in BCD
 */
void epochToBCDTime(uint64_t timeSec, uint8_t& seconds, uint8_t& minutes,
                    uint8_t& hours, uint8_t& day, uint8_t& month,
                    uint16_t& year);
} // namespace utils

} // namespace responder
} // namespace pldm
