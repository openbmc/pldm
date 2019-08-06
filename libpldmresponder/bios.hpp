#pragma once

#include "config.h"

#include "bios_parser.hpp"
#include "bios_table.hpp"
#include "registration.hpp"

#include <stdint.h>

#include <map>
#include <vector>

#include "libpldm/bios.h"

namespace pldm
{

using AttributeHandle = uint16_t;
using StringHandle = uint16_t;
using PossibleValuesByHandle = std::vector<StringHandle>;

namespace responder
{

namespace bios
{
/** @brief Register handlers for command from the platform spec
 */
void registerHandlers();

namespace detail
{

/** @brief Constructs all the BIOS Tables
 *
 *  @param[in] request - Request message
 *  @param[in] payload_length - Request message payload length
 *  @param[in] biosJsonDir - path to fetch the BIOS json files
 *  @param[in] biosTablePath - path where the BIOS tables will be persisted
 */
Response buildBIOSTables(const Interfaces& intfs, const Request& request,
                         size_t payloadLength, const char* biosJsonDir,
                         const char* biosTablePath);
} // namespace detail

} // namespace bios

/** @brief Handler for GetDateTime
 *
 *  @param[in] request - Request message payload
 *  @param[return] Response - PLDM Response message
 */
Response getDateTime(const Interfaces& intfs, const Request& request,
                     size_t payloadLength);

/** @brief Handler for GetBIOSTable
 *
 *  @param[in] request - Request message
 *  @param[in] payload_length - Request message payload length
 *  @param[return] Response - PLDM Response message
 */
Response getBIOSTable(const Interfaces& intfs, const Request& request,
                      size_t payloadLength);

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
