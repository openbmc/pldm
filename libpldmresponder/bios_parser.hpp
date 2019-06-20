#pragma once

#include <stdint.h>

#include <fstream>
#include <string>
#include <vector>

/*
 * Usage flow:
 *
 * 1) Invoke getBIOSStrings to get all the attribute names and the preconfigured
 *    associated with the BIOS tables. This will be used to populate the BIOS
 *    string table.
 *
 * 2) bios_enum::getBIOSValues(xyz/abc) will be invoked to populate the BIOS
 *    attribute table for BIOSEnumeration and BIOSEnumerationReadonly.
 *    Similarly namespaces and getBIOSValues function will be available for
 *    different types.
 *
 * 3) bios_enum::getAttrValue("attribute_name") will return the current values
 *    for the BIOS enumeration type. If there is no D-Bus mapping for the
 *    attribute then default value is returned.
 *
 */

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

namespace bios_enum
{

using AttrName = std::string;
using Value = std::string;
using PossibleValues = std::vector<std::string>;
using DefaultValues = std::vector<std::string>;
using CurrentValues = std::vector<std::string>;
using AttrValuesMap = std::map < AttrName,
      std::tuple<PossibleValues, DefaultValues>;
using PropertyValue = std::variant<bool, uint8_t, int16_t, uint16_t, int32_t,
                            uint32_t,int64_t, uint64_t, double, std::string>;

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
AttrValuesMap getBIOSValues(const char* dirPath);

/** @brief Get the current value for BIOS Attribute
 *
 * @param[in] attrName - BIOS attribute name
 *
 * @return BIOS attribute value
 */
CurrentValues getAttrValue(const std::string& attrName);

struct DBusMapping
{
    std::map<Value, PropertyValue> valToDBusValMap;
    std::string objectPath;
    std::string interface;
    std::string propertyName;
};

namespace dbus_mapping
{

namespace internal
{

std::map<AttrName, std::tuple<DefaultValues, std::optional<DBusMapping>> attrLookup;

} // namespace dbus_mapping

} // namespace internal



} // namespace bios_enum

} // namespace json_parser
} // namespace bios
