#pragma once

#include <map>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

/*
 * BIOS Parser API usage:
 *
 * 1) bios_parser::getStrings gets all the attribute names and the preconfigured
 *    strings used in representing the values of the attributes. This will be
 *    used to populate the BIOS String table.
 *
 * 2) bios_parser::setupConfig has to be invoked to setup the lookup data
 *    structure all the attributes of that type. This API needs to be invoked
 *    before invoking bios_enum::getValues and bios_enum::getAttrValue.
 *
 * 3) bios_enum::getValues is invoked to populate the BIOS attribute table for
 *    BIOSEnumeration and BIOSEnumerationReadonly types.(similar API for other
 *    BIOS attribute types)
 *
 * 4) bios_enum::getAttrValue will return the current values for the BIOS
 *    enumeration attribute. If there is no D-Bus mapping for the attribute then
 *    default value is returned.(similar API for other BIOS attribute types).
 *
 */

namespace bios_parser
{

using Strings = std::vector<std::string>;
inline constexpr auto bIOSEnumJson = "enum_attrs.json";
inline constexpr auto bIOSStrJson = "string_attrs.json";

/** @brief Get all the preconfigured strings
 *  @return all the preconfigurated strings
 */
const Strings& getStrings();
/** @brief Parse every BIOS Configuration JSON file in the directory path
 *  @param[in] dirPath - directory path where all the bios configuration JSON
 * files exist
 */
int setupConfig(const char* dirPath);

namespace bios_enum
{

using AttrName = std::string;
using IsReadOnly = bool;
using PossibleValues = std::vector<std::string>;
using DefaultValues = std::vector<std::string>;
using AttrValuesMap =
    std::map<AttrName, std::tuple<IsReadOnly, PossibleValues, DefaultValues>>;

/** @brief Get the possible values and the default values for the
 *         BIOSEnumeration and BIOSEnumerationReadOnly types
 *
 *  @return information needed to build the BIOS attribute table specific to
 *         BIOSEnumeration and BIOSEnumerationReadOnly types
 */
const AttrValuesMap& getValues();

using CurrentValues = std::vector<std::string>;

/** @brief Get the current values for the BIOS Attribute
 *
 *  @param[in] attrName - BIOS attribute name
 *
 *  @return BIOS attribute value
 */
CurrentValues getAttrValue(const AttrName& attrName);

} // namespace bios_enum

namespace bios_string
{

using AttrName = std::string;
using IsReadOnly = bool;
using StrType = uint8_t;
using MinStrLen = uint16_t;
using MaxStrLen = uint16_t;
using DefaultStrLen = uint16_t;
using DefaultStr = std::string;
using AttrValuesMap =
    std::map<AttrName, std::tuple<IsReadOnly, StrType, MinStrLen, MaxStrLen,
                                  DefaultStrLen, DefaultStr>>;

/** @brief Get the string related values and the default values for the
 *         BIOSString and BIOSStringReadOnly types
 *
 *  @return information needed to build the BIOS attribute table specific to
 *         BIOSString and BIOSStringReadOnly types
 */
const AttrValuesMap& getValues();

/** @brief Get the current values for the BIOS Attribute
 *
 *  @param[in] attrName - BIOS attribute name
 *
 *  @return BIOS attribute value
 */
std::string getAttrValue(const AttrName& attrName);

} // namespace bios_string

} // namespace bios_parser
