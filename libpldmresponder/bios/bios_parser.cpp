#include "bios_parser.hpp"

#include "bios_table.hpp"
#include "utils.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>

#include "libpldm/bios.h"
#include "libpldm/bios_table.h"

namespace bios_parser
{

using Json = nlohmann::json;
namespace fs = std::filesystem;
using namespace pldm::responder::bios;

const std::vector<Json> emptyJsonList{};
const Json emptyJson{};

struct DBusMapping
{
    std::string objectPath;   //!< D-Bus object path
    std::string interface;    //!< D-Bus interface
    std::string propertyName; //!< D-Bus property name
    std::string propertyType; //!< D-Bus property type
};

using AttrType = uint8_t;
using Table = std::vector<uint8_t>;
using BIOSJsonName = std::string;
using AttrLookup = std::map<AttrName, std::optional<DBusMapping>>;
using PropertyValue =
    std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t,
                 uint64_t, double, std::string>;
const std::set<std::string> SupportedDbusPropertyTypes = {
    "bool",     "uint8_t", "int16_t",  "uint16_t", "int32_t",
    "uint32_t", "int64_t", "uint64_t", "double",   "string"};

using BIOSStringHandler =
    std::function<int(const Json& entry, Strings& strings)>;
using AttrLookupHandler = std::function<int(const Json& entry, AttrLookup)>;
using typeHandler = std::function<int(const Json& entry)>;

Strings BIOSStrings;
AttrLookup BIOSAttrLookup;

const Strings& getStrings()
{
    return BIOSStrings;
}

int parseBIOSJsonFile(const fs::path& dirPath, const std::string& fileName,
                      Json& fileData)
{
    int rc = 0;

    fs::path filePath = dirPath / fileName;

    std::ifstream jsonFile(filePath);
    if (!jsonFile.is_open())
    {
        std::cerr << "BIOS config file does not exist, FILE="
                  << filePath.c_str() << "\n";
        rc = -1;
    }
    else
    {
        fileData = Json::parse(jsonFile, nullptr, false);
        if (fileData.is_discarded())
        {
            std::cerr << "Parsing config file failed, FILE=" << filePath.c_str()
                      << "\n";
            rc = -1;
        }
    }

    return rc;
}

namespace bios_enum
{

namespace internal
{

using Value = std::string;

/** @brief Map of DBus property value to attribute value
 */
using DbusValToValMap = std::map<PropertyValue, Value>;

/** @brief Map containing the DBus property value to attribute value map for the
 * BIOS enumeration type attributes
 */
std::map<AttrName, DbusValToValMap> dbusValToValMaps;

/** @brief Map containing the possible and the default values for the BIOS
 *         enumeration type attributes.
 */
AttrValuesMap valueMap;

/** @brief Populate the mapping between D-Bus property value and attribute value
 *         for the BIOS enumeration attribute.
 *
 *  @param[in] type - type of the D-Bus property
 *  @param[in] dBusValues - json array of D-Bus property values
 *  @param[in] pv - Possible values for the BIOS enumeration attribute
 *
 */
DbusValToValMap populateMapping(const std::string& type, const Json& dBusValues,
                                const PossibleValues& pv)
{
    size_t pos = 0;
    PropertyValue value;
    DbusValToValMap valueMap;
    for (auto it = dBusValues.begin(); it != dBusValues.end(); ++it, ++pos)
    {
        if (type == "uint8_t")
        {
            value = static_cast<uint8_t>(it.value());
        }
        else if (type == "uint16_t")
        {
            value = static_cast<uint16_t>(it.value());
        }
        else if (type == "uint32_t")
        {
            value = static_cast<uint32_t>(it.value());
        }
        else if (type == "uint64_t")
        {
            value = static_cast<uint64_t>(it.value());
        }
        else if (type == "int16_t")
        {
            value = static_cast<int16_t>(it.value());
        }
        else if (type == "int32_t")
        {
            value = static_cast<int32_t>(it.value());
        }
        else if (type == "int64_t")
        {
            value = static_cast<int64_t>(it.value());
        }
        else if (type == "bool")
        {
            value = static_cast<bool>(it.value());
        }
        else if (type == "double")
        {
            value = static_cast<double>(it.value());
        }
        else if (type == "string")
        {
            value = static_cast<std::string>(it.value());
        }
        else
        {
            std::cerr << "Unknown D-Bus property type, TYPE=" << type.c_str()
                      << "\n";
        }

        valueMap.emplace(value, pv[pos]);
    }

    return valueMap;
}

void updateDbusProperty(const DBusMapping& dBusMap, const PropertyValue& value)
{
    auto setDbusProperty = [&dBusMap](const auto& variant) {
        pldm::utils::DBusHandler().setDbusProperty(
            dBusMap.objectPath.c_str(), dBusMap.propertyName.c_str(),
            dBusMap.interface.c_str(), variant);
    };

    if (dBusMap.propertyType == "uint8_t")
    {
        std::variant<uint8_t> v = std::get<uint8_t>(value);
        setDbusProperty(v);
    }
    else if (dBusMap.propertyType == "int16_t")
    {
        std::variant<int16_t> v = std::get<int16_t>(value);
        setDbusProperty(v);
    }
    else if (dBusMap.propertyType == "uint16_t")
    {
        std::variant<uint16_t> v = std::get<uint16_t>(value);
        setDbusProperty(v);
    }
    else if (dBusMap.propertyType == "int32_t")
    {
        std::variant<int32_t> v = std::get<int32_t>(value);
        setDbusProperty(v);
    }
    else if (dBusMap.propertyType == "uint32_t")
    {
        std::variant<uint32_t> v = std::get<uint32_t>(value);
        setDbusProperty(v);
    }
    else if (dBusMap.propertyType == "int64_t")
    {
        std::variant<int64_t> v = std::get<int64_t>(value);
        setDbusProperty(v);
    }
    else if (dBusMap.propertyType == "uint64_t")
    {
        std::variant<uint64_t> v = std::get<uint64_t>(value);
        setDbusProperty(v);
    }
    else if (dBusMap.propertyType == "double")
    {
        std::variant<double> v = std::get<double>(value);
        setDbusProperty(v);
    }
    else if (dBusMap.propertyType == "string")
    {
        std::variant<std::string> v = std::get<std::string>(value);
        setDbusProperty(v);
    }
    else
    {
        assert(false && "UnSpported Dbus Type");
    }
}

} // namespace internal

int setupBIOSStrings(const Json& entry, Strings& strings)
{
    Json pvs = entry.value("possible_values", emptyJsonList);

    for (auto& pv : pvs)
    {
        strings.emplace_back(std::move(pv.get<std::string>()));
    }

    return 0;
}

int setup(const Json& entry)
{
    PossibleValues possibleValues;
    DefaultValues defaultValues;

    std::string attrName = entry.value("attribute_name", "");
    Json pv = entry["possible_values"];
    for (auto& val : pv)
    {
        possibleValues.emplace_back(std::move(val));
    }
    Json dv = entry["default_values"];
    for (auto& val : dv)
    {
        defaultValues.emplace_back(std::move(val));
    }
    if (entry.count("dbus") != 0)
    {
        auto dbusEntry = entry.value("dbus", emptyJson);
        std::string propertyType = dbusEntry.value("property_type", "");
        Json propValues = dbusEntry["property_values"];
        internal::dbusValToValMaps.emplace(
            attrName, internal::populateMapping(propertyType, propValues,
                                                possibleValues));
    }
    // Defaulting all the types of attributes to BIOSEnumeration
    internal::valueMap.emplace(std::move(attrName),
                               std::make_tuple(entry.count("dbus") == 0,
                                               std::move(possibleValues),
                                               std::move(defaultValues)));
    return 0;
}

const AttrValuesMap& getValues()
{
    return internal::valueMap;
}

CurrentValues getAttrValue(const AttrName& attrName)
{
    const auto& dBusMap = BIOSAttrLookup.at(attrName);
    CurrentValues currentValues;
    PropertyValue propValue;

    if (dBusMap == std::nullopt)
    {
        const auto& valueEntry = internal::valueMap.at(attrName);
        const auto& [readOnly, possibleValues, currentValues] = valueEntry;
        return currentValues;
    }

    const auto& dbusValToValMap = internal::dbusValToValMaps.at(attrName);
    propValue =
        pldm::utils::DBusHandler().getDbusPropertyVariant<PropertyValue>(
            dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
            dBusMap->interface.c_str());

    auto iter = dbusValToValMap.find(propValue);
    if (iter != dbusValToValMap.end())
    {
        currentValues.push_back(iter->second);
    }

    return currentValues;
}

int setAttrValue(const AttrName& attrName,
                 const pldm_bios_attr_val_table_entry* attrValueEntry,
                 const pldm_bios_attr_table_entry* attrEntry,
                 const BIOSStringTable& stringTable)
{
    const auto& dBusMap = BIOSAttrLookup.at(attrName);
    if (dBusMap == std::nullopt)
    {
        return PLDM_SUCCESS;
    }

    uint8_t pvNum = pldm_bios_table_attr_entry_enum_decode_pv_num(attrEntry);
    std::vector<uint16_t> pvHdls(pvNum, 0);
    pldm_bios_table_attr_entry_enum_decode_pv_hdls(attrEntry, pvHdls.data(),
                                                   pvNum);

    uint8_t defNum =
        pldm_bios_table_attr_value_entry_enum_decode_number(attrValueEntry);

    assert(defNum == 1);

    std::vector<uint8_t> currHdls(1, 0);
    pldm_bios_table_attr_value_entry_enum_decode_handles(
        attrValueEntry, currHdls.data(), currHdls.size());

    auto valueString = stringTable.findString(pvHdls[currHdls[0]]);

    const auto& dbusValToValMap = internal::dbusValToValMaps.at(attrName);

    auto it = std::find_if(dbusValToValMap.begin(), dbusValToValMap.end(),
                           [&valueString](const auto& typePair) {
                               return typePair.second == valueString;
                           });
    if (it == dbusValToValMap.end())
    {
        std::cerr << "Invalid Enum Value\n";
        return PLDM_ERROR;
    }

    internal::updateDbusProperty(dBusMap.value(), it->first);

    return PLDM_SUCCESS;
}

} // namespace bios_enum

namespace bios_string
{

/** @brief BIOS string types
 */
enum BIOSStringEncoding
{
    UNKNOWN = 0x00,
    ASCII = 0x01,
    HEX = 0x02,
    UTF_8 = 0x03,
    UTF_16LE = 0x04,
    UTF_16BE = 0x05,
    VENDOR_SPECIFIC = 0xFF
};

const std::map<std::string, uint8_t> strTypeMap{
    {"Unknown", UNKNOWN},
    {"ASCII", ASCII},
    {"Hex", HEX},
    {"UTF-8", UTF_8},
    {"UTF-16LE", UTF_16LE},
    {"UTF-16LE", UTF_16LE},
    {"Vendor Specific", VENDOR_SPECIFIC}};

namespace internal
{

/** @brief Map containing the possible and the default values for the BIOS
 *         string type attributes.
 */
AttrValuesMap valueMap;

} // namespace internal

int setup(const Json& jsonEntry)
{

    std::string attr = jsonEntry.value("attribute_name", "");
    // Transfer string type from string to enum
    std::string strTypeTmp = jsonEntry.value("string_type", "Unknown");
    auto iter = strTypeMap.find(strTypeTmp);
    if (iter == strTypeMap.end())
    {
        std::cerr << "Wrong string type, STRING_TYPE=" << strTypeTmp.c_str()
                  << " ATTRIBUTE_NAME=" << attr.c_str() << "\n";
        return -1;
    }
    uint8_t strType = iter->second;

    uint16_t minStrLen = jsonEntry.value("minimum_string_length", 0);
    uint16_t maxStrLen = jsonEntry.value("maximum_string_length", 0);
    uint16_t defaultStrLen = jsonEntry.value("default_string_length", 0);
    std::string defaultStr = jsonEntry.value("default_string", "");

    pldm_bios_table_attr_entry_string_info info = {
        0,     /* name handle */
        false, /* read only */
        strType, minStrLen, maxStrLen, defaultStrLen, defaultStr.data(),
    };

    const char* errmsg;
    auto rc = pldm_bios_table_attr_entry_string_info_check(&info, &errmsg);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Wrong filed for string attribute, ATTRIBUTE_NAME="
                  << attr.c_str() << " ERRMSG=" << errmsg
                  << " MINIMUM_STRING_LENGTH=" << minStrLen
                  << " MAXIMUM_STRING_LENGTH=" << maxStrLen
                  << " DEFAULT_STRING_LENGTH=" << defaultStrLen
                  << " DEFAULT_STRING=" << defaultStr.data() << "\n";
        return -1;
    }

    // Defaulting all the types of attributes to BIOSString
    internal::valueMap.emplace(
        std::move(attr),
        std::make_tuple(jsonEntry.count("dbus") == 0, strType, minStrLen,
                        maxStrLen, defaultStrLen, std::move(defaultStr)));

    return 0;
}

const AttrValuesMap& getValues()
{
    return internal::valueMap;
}

std::string getAttrValue(const AttrName& attrName)
{
    const auto& dBusMap = BIOSAttrLookup.at(attrName);
    std::variant<std::string> propValue;

    if (dBusMap == std::nullopt)
    { // return default string
        const auto& valueEntry = internal::valueMap.at(attrName);
        return std::get<DefaultStr>(valueEntry);
    }

    return pldm::utils::DBusHandler().getDbusProperty<std::string>(
        dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
        dBusMap->interface.c_str());
}

std::string stringToUtf8(BIOSStringEncoding stringType,
                         const std::vector<uint8_t>& data)
{
    switch (stringType)
    {
        case ASCII:
        case UTF_8:
        case HEX:
            return std::string(data.begin(), data.end());
        case UTF_16BE:
        case UTF_16LE: // TODO
            return std::string(data.begin(), data.end());
        case VENDOR_SPECIFIC:
            throw std::invalid_argument("Vendor Specific is unsupported");
        case UNKNOWN:
            throw std::invalid_argument("Unknown String Type");
    }
    throw std::invalid_argument("String Type Error");
}

int setAttrValue(const AttrName& attrName,
                 const pldm_bios_attr_val_table_entry* attrValueEntry,
                 const pldm_bios_attr_table_entry* attrEntry,
                 const BIOSStringTable&)
{
    const auto& dBusMap = BIOSAttrLookup.at(attrName);
    if (dBusMap == std::nullopt)
    {
        return PLDM_SUCCESS;
    }

    auto stringType =
        pldm_bios_table_attr_entry_string_decode_string_type(attrEntry);

    variable_field currentString{};
    pldm_bios_table_attr_value_entry_string_decode_string(attrValueEntry,
                                                          &currentString);
    std::vector<uint8_t> data(currentString.ptr,
                              currentString.ptr + currentString.length);

    std::variant<std::string> value =
        stringToUtf8(static_cast<BIOSStringEncoding>(stringType), data);

    pldm::utils::DBusHandler().setDbusProperty(
        dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
        dBusMap->interface.c_str(), value);

    return PLDM_SUCCESS;
}

} // namespace bios_string

namespace bios_integer
{

AttrValuesMap valueMap;

int setup(const Json& jsonEntry)
{

    std::string attr = jsonEntry.value("attribute_name", "");
    // Transfer string type from string to enum

    uint64_t lowerBound = jsonEntry.value("lower_bound", 0);
    uint64_t upperBound = jsonEntry.value("upper_bound", 0);
    uint32_t scalarIncrement = jsonEntry.value("scalar_increment", 1);
    uint64_t defaultValue = jsonEntry.value("default_value", 0);
    pldm_bios_table_attr_entry_integer_info info = {
        0,     /* name handle*/
        false, /* read only */
        lowerBound, upperBound, scalarIncrement, defaultValue,
    };
    const char* errmsg = nullptr;
    auto rc = pldm_bios_table_attr_entry_integer_info_check(&info, &errmsg);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Wrong filed for integer attribute, ATTRIBUTE_NAME="
                  << attr.c_str() << " ERRMSG=" << errmsg
                  << " LOWER_BOUND=" << lowerBound
                  << " UPPER_BOUND=" << upperBound
                  << " DEFAULT_VALUE=" << defaultValue
                  << " SCALAR_INCREMENT=" << scalarIncrement << "\n";
        return -1;
    }

    valueMap.emplace(std::move(attr),
                     std::make_tuple(jsonEntry.count("dbus") == 0, lowerBound,
                                     upperBound, scalarIncrement,
                                     defaultValue));

    return 0;
}

void updateDbusProperty(const DBusMapping& dBusMap, uint64_t value)
{
    auto setDbusProperty = [&dBusMap](const auto& variant) {
        pldm::utils::DBusHandler().setDbusProperty(
            dBusMap.objectPath.c_str(), dBusMap.propertyName.c_str(),
            dBusMap.interface.c_str(), variant);
    };

    if (dBusMap.propertyType == "uint8_t")
    {
        std::variant<uint8_t> v = value;
        setDbusProperty(v);
    }
    else if (dBusMap.propertyType == "int16_t")
    {
        std::variant<int16_t> v = value;
        setDbusProperty(v);
    }
    else if (dBusMap.propertyType == "uint16_t")
    {
        std::variant<uint16_t> v = value;
        setDbusProperty(v);
    }
    else if (dBusMap.propertyType == "int32_t")
    {
        std::variant<int32_t> v = value;
        setDbusProperty(v);
    }
    else if (dBusMap.propertyType == "uint32_t")
    {
        std::variant<uint32_t> v = value;
        setDbusProperty(v);
    }
    else if (dBusMap.propertyType == "int64_t")
    {
        std::variant<int64_t> v = value;
        setDbusProperty(v);
    }
    else if (dBusMap.propertyType == "uint64_t")
    {
        std::variant<uint64_t> v = value;
        setDbusProperty(v);
    }
    else
    {
        assert(false && "Unsupported Dbus Type");
    }
}

const AttrValuesMap& getValues()
{
    return valueMap;
}

uint64_t getAttrValue(const AttrName& attrName)
{
    const auto& dBusMap = BIOSAttrLookup.at(attrName);
    std::variant<std::string> propValue;

    if (dBusMap == std::nullopt)
    { // return default string
        const auto& valueEntry = valueMap.at(attrName);
        return std::get<AttrDefaultValue>(valueEntry);
    }

    return pldm::utils::DBusHandler().getDbusProperty<uint64_t>(
        dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
        dBusMap->interface.c_str());
}

int setAttrValue(const AttrName& attrName,
                 const pldm_bios_attr_val_table_entry* attrValueEntry,
                 const pldm_bios_attr_table_entry*, const BIOSStringTable&)
{
    const auto& dBusMap = BIOSAttrLookup.at(attrName);
    if (dBusMap == std::nullopt)
    {
        return PLDM_SUCCESS;
    }

    uint64_t currentValue =
        pldm_bios_table_attr_value_entry_integer_decode_cv(attrValueEntry);

    updateDbusProperty(dBusMap.value(), currentValue);

    return PLDM_SUCCESS;
}

} // namespace bios_integer

const std::map<BIOSJsonName, BIOSStringHandler> BIOSStringHandlers = {
    {bIOSEnumJson, bios_enum::setupBIOSStrings},
};

const std::map<BIOSJsonName, typeHandler> BIOSTypeHandlers = {
    {bIOSEnumJson, bios_enum::setup},
    {bIOSStrJson, bios_string::setup},
    {bIOSIntegerJson, bios_integer::setup},
};

void setupBIOSStrings(const BIOSJsonName& jsonName, const Json& entry,
                      Strings& strings)
{
    strings.emplace_back(entry.value("attribute_name", ""));
    auto iter = BIOSStringHandlers.find(jsonName);
    if (iter != BIOSStringHandlers.end())
    {
        iter->second(entry, strings);
    }
}

void setupBIOSAttrLookup(const Json& jsonEntry, AttrLookup& lookup)
{
    std::optional<DBusMapping> dBusMap;
    std::string attrName = jsonEntry.value("attribute_name", "");

    if (jsonEntry.count("dbus") != 0)
    {
        auto dBusEntry = jsonEntry.value("dbus", emptyJson);
        std::string objectPath = dBusEntry.value("object_path", "");
        std::string interface = dBusEntry.value("interface", "");
        std::string propertyName = dBusEntry.value("property_name", "");
        std::string propertyType = dBusEntry.value("property_type", "");
        if (!objectPath.empty() && !interface.empty() &&
            !propertyName.empty() &&
            (SupportedDbusPropertyTypes.find(propertyType) !=
             SupportedDbusPropertyTypes.end()))
        {
            dBusMap = std::optional<DBusMapping>(
                {objectPath, interface, propertyName, propertyType});
        }
        else
        {
            std::cerr << "Invalid dbus config, OBJPATH="
                      << dBusMap->objectPath.c_str()
                      << " INTERFACE=" << dBusMap->interface.c_str()
                      << " PROPERTY_NAME=" << dBusMap->propertyName.c_str()
                      << " PROPERTY_TYPE=" << dBusMap->propertyType.c_str()
                      << "\n";
        }
    }
    lookup.emplace(attrName, dBusMap);
}

int setupBIOSType(const BIOSJsonName& jsonName, const Json& entry)
{
    auto iter = BIOSTypeHandlers.find(jsonName);
    if (iter != BIOSTypeHandlers.end())
    {
        iter->second(entry);
    }
    return 0;
}

const std::vector<BIOSJsonName> BIOSConfigFiles = {bIOSEnumJson, bIOSStrJson,
                                                   bIOSIntegerJson};

int setupConfig(const char* dirPath)
{
    if (!BIOSStrings.empty() && !BIOSAttrLookup.empty())
    {
        return 0;
    }

    fs::path dir(dirPath);
    if (!fs::exists(dir) || fs::is_empty(dir))
    {
        std::cerr << "BIOS config directory does not exist or empty, DIR="
                  << dirPath << "\n";
        return -1;
    }
    for (auto jsonName : BIOSConfigFiles)
    {
        Json json;
        if (parseBIOSJsonFile(dir, jsonName, json) < 0)
        {
            continue;
        }
        auto entries = json.value("entries", emptyJsonList);
        for (auto& entry : entries)
        {
            setupBIOSStrings(jsonName, entry, BIOSStrings);
            setupBIOSAttrLookup(entry, BIOSAttrLookup);
            setupBIOSType(jsonName, entry);
        }
    }
    if (BIOSStrings.empty())
    { // means there is no attribute
        std::cerr << "No attribute is found in the config directory, DIR="
                  << dirPath << "\n";
        return -1;
    }
    return 0;
}

using setAttrValueHandler = std::function<int(
    const AttrName&, const pldm_bios_attr_val_table_entry*,
    const pldm_bios_attr_table_entry*, const BIOSStringTable&)>;

const std::map<AttrType, setAttrValueHandler> SetAttrValueMap{{
    {PLDM_BIOS_STRING, bios_string::setAttrValue},
    {PLDM_BIOS_STRING_READ_ONLY, bios_string::setAttrValue},
    {PLDM_BIOS_ENUMERATION, bios_enum::setAttrValue},
    {PLDM_BIOS_ENUMERATION_READ_ONLY, bios_enum::setAttrValue},
    {PLDM_BIOS_INTEGER, bios_integer::setAttrValue},
    {PLDM_BIOS_INTEGER_READ_ONLY, bios_integer::setAttrValue},

}};

int setAttributeValueOnDbus(const variable_field* attributeData,
                            const BIOSTable& biosAttributeTable,
                            const BIOSStringTable& stringTable)
{
    Table attributeTable;
    biosAttributeTable.load(attributeTable);
    auto attrValueEntry =
        reinterpret_cast<const pldm_bios_attr_val_table_entry*>(
            attributeData->ptr);

    auto attrType =
        pldm_bios_table_attr_value_entry_decode_attribute_type(attrValueEntry);
    auto attrHandle = pldm_bios_table_attr_value_entry_decode_attribute_handle(
        attrValueEntry);

    auto attrEntry = pldm_bios_table_attr_find_by_handle(
        attributeTable.data(), attributeTable.size(), attrHandle);

    assert(attrEntry != nullptr);

    auto attrNameHandle =
        pldm_bios_table_attr_entry_decode_string_handle(attrEntry);

    auto attrName = stringTable.findString(attrNameHandle);

    try
    {
        auto rc = SetAttrValueMap.at(attrType)(attrName, attrValueEntry,
                                               attrEntry, stringTable);
        return rc;
    }
    catch (const std::exception& e)
    {
        std::cerr << "setAttributeValueOnDbus Error: " << e.what() << std::endl;
        return PLDM_ERROR;
    }
}

} // namespace bios_parser
