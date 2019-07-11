#pragma once

#include "config.h"

#include "bios_parser.hpp"
#include "bios_table.hpp"

#include <stdint.h>

#include <map>
#include <vector>

#include "libpldm/bios.h"

using namespace pldm::responder::bios;
using namespace bios_parser;
using namespace bios_parser::bios_enum;

namespace pldm
{

using Response = std::vector<uint8_t>;
using handle = uint16_t;

namespace responder
{

namespace bios
{
/** @brief Register handlers for command from the platform spec
 */
void registerHandlers();
} // namespace bios

using PossibleValuesByHandle = std::vector<handle>;
using DefaultValuesByHandle = std::vector<handle>;

/** @brief Handler for GetDateTime
 *
 *  @param[in] request - Request message payload
 *  @param[return] Response - PLDM Response message
 */
Response getDateTime(const pldm_msg* request, size_t payloadLength);

/** @brief Handler for GetBIOSTable
 *
 *  @param[in] request - Request message
 *  @param[in] payload_length - Request message payload length
 *  @param[return] Response - PLDM Response message
 */
Response getBIOSTable(const pldm_msg* request, size_t payloadLength);

/** @brief Constructs all the BIOS Tables
 *
 *  @param[in] request - Request message
 *  @param[in] payload_length - Request message payload length
 *  @param[in] path - path where the BIOS tables will be persisted
 */
Response buildBIOSTables(const pldm_msg* request, size_t payloadLength,
                         const char* biosJsonDir, const char* biosTablePath);

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
