#pragma once

#include <map>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

namespace bios_parser
{

using Strings = std::vector<std::string>;

/** @brief Parse every BIOS configuration JSON files in the directory path
 *         and populate all the attribute names and all the preconfigured
 *         strings used in representing the values of attributes.
 *
 * @param[in] dirPath - directory path where all the BIOS configuration JSON
 *                      files exist.
 *
 * @return all the strings that should be populated in the BIOS string table
 */
Strings getStrings(const char* dirPath);

namespace bios_enum
{

using AttrName = std::string;
using ReadOnly = bool;
using PossibleValues = std::vector<std::string>;
using DefaultValues = std::vector<std::string>;
using AttrValuesMap =
    std::map<AttrName, std::tuple<ReadOnly, PossibleValues, DefaultValues>>;

/** @brief Parse the JSON file specific to BIOSEnumeration and
 *         BIOSEnumerationReadOnly types and populate the attribute and
 *         the corresponding possible values and the default value.
 *
 * @param[in] dirPath - directory path where all the BIOS configuration JSON
 *                      exist
 *
 * @return information needed to build the BIOS attribute table specific to
 *         BIOSEnumeration and BIOSEnumerationReadOnly types
 */
AttrValuesMap getValues(const char* dirPath);

using CurrentValues = std::vector<std::string>;

/** @brief Get the current values for the BIOS Attribute
 *
 * @param[in] attrName - BIOS attribute name
 *
 * @return BIOS attribute value
 */
CurrentValues getAttrValue(const std::string& attrName);

} // namespace bios_enum

} // namespace bios_parser
