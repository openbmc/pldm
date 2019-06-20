#pragma once

#include <stdint.h>

#include <fstream>
#include <string>
#include <vector>

namespace bios
{
namespace json_parser
{

using Strings = std::vector<std::string>;

/** @brief Parse every BIOS configuration JSON files in the file path
 *         and populate all the attribute names and all the preconfigured
 *         strings used in representing the values of attributes.
 *
 * @param[in] filePath - file path where all the BIOS configuration JSON files
 *                        exist
 *
 * @return all the strings that should be populated in the BIOS string table
 *
 */
Strings getBIOSStrings(const char* filePath);

using AttrName = std::string;
using PossibleValues = std::vector<std::string>;
using DefaultValues = std::vector<std::string>;
using AttrValuesMap = std::map < AttrName,
      std::tuple<PossibleValues, DefaultValues>;

/** @brief Parse the JSON file specific to BIOSEnumeration and
 *         BIOSEnumerationReadOnly types and populate the attribute and
 *         the corresponding possible values and the default value.
 *
 * @param[in] filePath - file path where all the BIOS configuration JSON files
 *                        exist
 *
 * @return information needed to build the BIOS attribute table specific to
 *         BIOSEnumeration and BIOSEnumerationReadOnly types
 */
AttrValuesMap getBIOSEnumValues(const char* filePath);

} // namespace json_parser
} // namespace bios
