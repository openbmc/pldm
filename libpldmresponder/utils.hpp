#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace pldm
{
namespace responder
{

/** @brief Format time to sec:min:hr:day:month:year format
 *
 *  @param[in] s - Time got from epoch time in format Www Mmm dd hh:mm:ss yyyy
 *  @param[out] seconds - number of seconds
 *  @param[out] minutes - number of minutes
 *  @param[out] hours - number of hours
 *  @param[out] month - month number
 *  @param[out] day - day of the month
 *  @param[out] year - year number
 */
void formatTime(const std::string& s, uint8_t& seconds, uint8_t& minutes,
                uint8_t& hours, uint8_t& day, uint8_t& month, uint16_t& year);

} // namespace responder
} // namespace pldm
