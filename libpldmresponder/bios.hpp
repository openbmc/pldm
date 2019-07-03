#pragma once

#include "bios_table.hpp"

#include <stdint.h>

#include <map>
#include <vector>

#include "libpldm/bios.h"

// using namespace pldm::responder;
using namespace pldm::responder::bios;
namespace pldm
{

using namespace std;
using Response = std::vector<uint8_t>;
using handle = uint16_t;

namespace responder
{

using attributNameHandle = uint16_t;
using PossibleValuesByHandle = std::vector<handle>;

using DefaultValuesByHandle = std::vector<handle>;

using AttributeMetaData =
    std::tuple<PossibleValuesByHandle, DefaultValuesByHandle>;

using AttrValuesMapByHandle = std::map<attributNameHandle, AttributeMetaData>;

using Strings =
    std::vector<std::string>; // to be removed after adding Tom's patch
using AttrName = std::string; // to be removed after adding Tom's patch
using PossibleValues =
    std::vector<std::string>; // to be removed after adding Tom's patch
using CurrentValues =
    std::vector<std::string>; // to be removed after adding Tom's patch

using DefaultValues =
    std::vector<std::string>; // to be removed after adding Tom's patch
using AttrValuesMap =
    std::map<AttrName,
             std::tuple<PossibleValues, DefaultValues>>; // to be removed after
                                                         // adding Tom's patch
#define BIOS_STRING_TABLE_FILE_PATH                                            \
    "/esw/san5/sampmisr/host_to_bmc/obmc/GetBIOSTable/pldm/test/bios/"         \
    "stringTableFile"
#define BIOS_ATTRIBUTE_TABLE_FILE_PATH                                         \
    "/esw/san5/sampmisr/host_to_bmc/obmc/GetBIOSTable/pldm/test/bios/"         \
    "attributeTableFile"
#define BIOS_ATTRIBUTE_VALUE_TABLE_PATH                                        \
    "/esw/san5/sampmisr/host_to_bmc/obmc/GetBIOSTable/pldm/test/bios/"         \
    "attributeValueTableFile"

/** @brief Handler for GetDateTime
 *
 *  @param[in] request - Request message payload
 *  @param[return] Response - PLDM Response message
 */
Response getDateTime(const pldm_msg* request);

/** @brief Handler for GetBIOSTable
 *
 *  @param[in] request - Request message payload
 *  @param[in] payload_length - Request message payload length
 *  @param[return] Response - PLDM Response message
 */

Response getBIOSTable(const pldm_msg* request, size_t payloadLength);

/** @brief Get next available handle to assign to a BIOS string
 *
 *  @return  uint16_t - string handle
 */
handle nextStringHandle();

handle nextAttrHandle();

Response getBIOSStringTable(bios::BIOSTable& BIOSStringTable,
                            uint32_t& transferHandle, uint8_t& transferOpFlag);

Response getBIOSAttributeTable(bios::BIOSTable& BIOSAttributeTable,
                               bios::BIOSTable& BIOSStringTable,
                               uint32_t& transferHandle,
                               uint8_t& transferOpFlag);
Response getBIOSAttributeValueTable(bios::BIOSTable& BIOSAttributeValueTable,
                                    bios::BIOSTable& BIOSAttributeTable,
                                    bios::BIOSTable& BIOSStringTable,
                                    uint32_t& transferHandle,
                                    uint8_t& transferOpFlag);

handle findStringHandle(std::string name, bios::BIOSTable& BIOSStringTable);

std::vector<handle> findStringHandle(std::vector<std::string> values,
                                     bios::BIOSTable& BIOSStringTable);

std::string findStringName(handle stringHandle,
                           bios::BIOSTable& BIOSStringTable);

std::vector<uint8_t> findDefaultValHandle(PossibleValues possiVals,
                                          DefaultValues defVals);

std::vector<uint8_t> findStrIndices(PossibleValuesByHandle possiVals,
                                    CurrentValues currVals,
                                    bios::BIOSTable& BIOSStringTable);

bios::Table getBIOSAttributeEnumerationTable(bios::BIOSTable& BIOSStringTable);
bios::Table
    getBIOSAttributeValueEnumerationTable(bios::BIOSTable& BIOSAttributeTable,
                                          bios::BIOSTable& BIOSStringTable);

Strings getBIOSStrings(const char* dirPath); // to be removed after Tom's patch
AttrValuesMap
    getBIOSValues(const char* dirPath); // to be removed after Tom's patch
CurrentValues getAttrValue(
    const std::string& attrName); // to be removed after Tom's patch

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
