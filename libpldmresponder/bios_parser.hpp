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
 * @param[in] dirPath - directory path where all the BIOS configuration JSON
 *                      files exist.
 *
 * @return all the strings that should be populated in the BIOS string table
 *
 */
Strings getBIOSStrings(const char* dirPath);

using AttrName = std::string;
using PossibleValues = std::vector<std::string>;
using DefaultValues = std::vector<std::string>;
using AttrValuesMap = std::map < AttrName,
      std::tuple<PossibleValues, DefaultValues>;

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
AttrValuesMap getBIOSEnumValues(const char* dirPath);

using PropertyValue =
    std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t,
                 uint64_t, double, std::string>;

using DbusObjectPath = std::string;
using DbusInterface = std::string;
using DbusProperty = std::string;
using DBusLookup = std::tuple<DbusObjectPath, DbusInterface, DbusProperty>;
using AttrValue =
    std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t,
                 uint64_t, double, std::string, DBusLookup>;

std::map<AttrName, AttrValue> AttrToValue{};

/** @brief Get the D-Bus property value corresponding to the BIOS
 * attribute name
 *
 * @param[in] attrName - bios attribute name
 *
 * @return property value
 *
 */
PropertyValue getDBusValue(const std::string& attrName);

} // namespace json_parser
} // namespace bios
