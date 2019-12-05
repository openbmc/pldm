#pragma once

#include "config.h"

#include "bios_parser.hpp"
#include "bios_table.hpp"
#include "handler.hpp"

#include <stdint.h>

#include <functional>
#include <map>
#include <vector>

#include "libpldm/bios.h"
#include "libpldm/bios_table.h"

namespace pldm
{

using AttributeHandle = uint16_t;
using StringHandle = uint16_t;
using PossibleValuesByHandle = std::vector<StringHandle>;

namespace responder
{

namespace bios
{

using AttrTableEntryHandler =
    std::function<void(const struct pldm_bios_attr_table_entry*)>;

void traverseBIOSAttrTable(const bios::Table& BIOSAttrTable,
                           AttrTableEntryHandler handler);

namespace internal
{

/** @brief Constructs all the BIOS Tables
 *
 *  @param[in] request - Request message
 *  @param[in] payload_length - Request message payload length
 *  @param[in] biosJsonDir - path to fetch the BIOS json files
 *  @param[in] biosTablePath - path where the BIOS tables will be persisted
 */
Response buildBIOSTables(const pldm_msg* request, size_t payloadLength,
                         const char* biosJsonDir, const char* biosTablePath);
} // namespace internal

class Handler : public CmdHandler
{
  public:
    Handler();

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

    /** @brief Handler for GetBIOSAttributeCurrentValueByHandle
     *
     *  @param[in] request - Request message
     *  @param[in] payloadLength - Request message payload length
     *  @return Response - PLDM Response message
     */
    Response getBIOSAttributeCurrentValueByHandle(const pldm_msg* request,
                                                  size_t payloadLength);
};

} // namespace bios

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
