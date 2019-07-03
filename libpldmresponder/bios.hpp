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

using namespace std;

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
 *  @param[in] request - Request message payload
 *  @param[in] payload_length - Request message payload length
 *  @param[return] Response - PLDM Response message
 */

Response getBIOSTable(const pldm_msg* request, size_t payloadLength);

/** @brief Constructs all the BIOS Tables
 *
 *  @param[in] request - Request message payload
 *  @param[in] payload_length - Request message payload length
 *  @param[in] biosJsonDir - place where BIOS json files are present
 *  @param[in] biosTablePath - path where the BIOS tables will be persisted
 */
Response buildBIOSTables(const pldm_msg* request, size_t payloadLength,
                         const char* biosJsonDir, const char* biosTablePath);

/** @brief Get next available handle to assign to a BIOS string
 *
 *  @return  uint16_t - string handle
 */
handle nextAttrHandle();

/** @brief Construct the BIOS attribute table
 *
 *  @param[in] BIOSAttributeTable - in memory copy of the attribute table
 *  @param[in] BIOSStringTable - in memory copy of the string table
 *  @param[in] transferHandle - transfer handle to identify part of transfer
 *  @param[in] transferOpFlag - flag to indicate which part of data being
 * transferred
 *  @param[in] instanceID - instance ID to identify the command
 *  @param[in] path - path where the BIOS tables will be persisted
 */
Response getBIOSAttributeTable(bios::BIOSTable& BIOSAttributeTable,
                               bios::BIOSTable& BIOSStringTable,
                               uint32_t& transferHandle,
                               uint8_t& transferOpFlag, uint8_t instanceID,
                               const char* path);

/** @brief Construct the BIOS attribute value table
 *
 *  @param[in] BIOSAttributeValueTable - in memory copy of the attribute value
 * table
 *  @param[in] BIOSAttributeTable - in memory copy of the attribute table
 *  @param[in] BIOSStringTable - in memory copy of the string table
 *  @param[in] transferHandle - transfer handle to identify part of transfer
 *  @param[in] transferOpFlag - flag to indicate which part of data being
 * transferred
 *  @param[in] instanceID - instance ID to identify the command
 *  @param[in] path - path where the BIOS tables will be persisted
 */
Response getBIOSAttributeValueTable(bios::BIOSTable& BIOSAttributeValueTable,
                                    bios::BIOSTable& BIOSAttributeTable,
                                    bios::BIOSTable& BIOSStringTable,
                                    uint32_t& transferHandle,
                                    uint8_t& transferOpFlag, uint8_t instanceID,
                                    const char* path);

/** @brief Find the string handle from the BIOS string table given the name
 *
 *  @param[in] name - name of the string
 *  @param[in] BIOSStringTable - in memory copy of the string table
 *
 *  @return - uint16_t - handle of the string
 */

handle findStringHandle(std::string name, bios::BIOSTable& BIOSStringTable);

/** @brief Find the string handles from the BIOS string table for a set of names
 *
 *  @param[in] values - vector of string names
 *  @param[in] BIOSStringTable - in memory copy of the string table
 *
 *  @return - std::vector<handle> - vector of handles corresponding to the names
 */
std::vector<handle> findStringHandle(std::vector<std::string> values,
                                     bios::BIOSTable& BIOSStringTable);

/** @brief Find the string name from the BIOS string table for a string handle
 *
 *  @param[in] stringHandle - string handle
 *  @param[in] BIOSStringTable - in memory copy of the string table
 *
 *  @return - std::string - name of the corresponding string
 */
std::string findStringName(handle stringHandle,
                           bios::BIOSTable& BIOSStringTable);

/** @brief Find the indices into the array of the possible values of string
 *  handles for the default values. This is used in attribute table
 *
 *  @param[in] possiVals - vector of strings comprising all the possible values
 *                         for an attribute
 *  @param[in] defVals - vector of strings comprising all the default values
 *                       for an attribute
 *  @return - std::vector<uint8_t> - indices into the array of the possible
 * values of string
 */
std::vector<uint8_t> findDefaultValHandle(PossibleValues possiVals,
                                          DefaultValues defVals);

/** @brief Find the indices  into the array of the possible values of string
 *  handles for the current values.This is used in attribute value table.
 *
 *  @param[in] possiVals - vector of string handles comprising all the possible
 *                        values for an attribute
 *  @param[in] currVals - vector of strings comprising all current values
 *                        for an attribute
 *  @param[in] BIOSStringTable - in memory copy of the string table
 *
 *  @return - std::vector<uint8_t> - indices into the array of the possible
 *  values of string handles
 */
std::vector<uint8_t> findStrIndices(PossibleValuesByHandle possiVals,
                                    CurrentValues currVals,
                                    bios::BIOSTable& BIOSStringTable);

/** @brief Construct the attibute table for BIOS type Enumeration and
 *  Enumeration ReadOnly.
 *
 *  @param[in] BIOSStringTable - in memory copy of the string table
 *  @param[in] path - place where the table will be persisted for future use
 *
 *  @return - Table - the attribute eenumeration table
 */
bios::Table getBIOSAttributeEnumerationTable(bios::BIOSTable& BIOSStringTable,
                                             const char* path);

/** @brief Construct the attibute value table for BIOS type Enumeration and
 *  Enumeration ReadOnly
 *
 *  @param[in] BIOSAttributeTable - in memory copy of the attribute table
 *  @param[in] BIOSStringTable - in memory copy of the string table
 *
 *  @return - Table - the attribute value table
 */
bios::Table
    getBIOSAttributeValueEnumerationTable(bios::BIOSTable& BIOSAttributeTable,
                                          bios::BIOSTable& BIOSStringTable);

/** @brief Calculate the pad for BIOS tables
 *
 *  @param[in] tableLen - Length of the table
 *
 *  @return - uint8_t - number of pad bytes
 */

uint8_t calculatePad(uint32_t tableLen);

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
