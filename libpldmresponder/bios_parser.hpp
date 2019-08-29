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
 * 2) bios_enum::setupValueLookup (similar API for other supported BIOS
 *    attribute types) has to be invoked to setup the lookup data structure for
 *    all the attributes of that type. This API needs to be invoked before
 *    invoking bios_enum::getValues and bios_enum::getAttrValue.
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

/** @brief Parse every BIOS configuration JSON files in the directory path
 *         and populate all the attribute names and all the preconfigured
 *         strings used in representing the values of attributes.
 *
 *  @param[in] dirPath - directory path where all the BIOS configuration JSON
 *                      files exist.
 *
 *  @return all the strings that should be populated in the BIOS string table
 */
Strings getStrings(const char* dirPath);

namespace bios_enum
{

/** @brief Parse the JSON file specific to BIOSEnumeration and
 *         BIOSEnumerationReadOnly types and populate the data structure for
 *         the corresponding possible values and the default value. Setup the
 *         data structure to lookup the current value of the BIOS enumeration
 *         attribute. JSON is parsed once and the information is cached.
 *
 *  @param[in] dirPath - directory path where all the BIOS configuration JSON
 *                      exist
 *
 *  @return 0 for success and negative return code for failure
 */
int setupValueLookup(const char* dirPath);

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

namespace bios_integer
{

int setupValueLookup(const char* dirPath);

using AttrName = std::string;
using LowerBound = uint64_t;
using UpperBound = uint64_t;
using ScalarIncrement = uint64_t;
using DefaultValue = uint64_t;
using AttrValues =
    std::tuple<LowerBound, UpperBound, ScalarIncrement, DefaultValue>;
using PossibleValues = std::vector<DefaultValue>;
constexpr auto AttrLowerBound = 0;
constexpr auto AttrUpperBound = 1;
constexpr auto AttrScalarIncrement = 2;
constexpr auto AttrDefaultValue = 3;

using AttrValuesMap = std::map<AttrName, AttrValues>;

const AttrValuesMap& getValues();

} // namespace bios_integer

} // namespace bios_parser
