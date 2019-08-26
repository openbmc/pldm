#include "bios_parser.hpp"

#include "libpldmresponder/utils.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <phosphor-logging/log.hpp>

namespace bios_parser
{

using Json = nlohmann::json;
namespace fs = std::filesystem;
using namespace phosphor::logging;

namespace
{

const std::vector<Json> emptyJsonList{};
const Json emptyJson{};

} // namespace

int parseBiosJsonFile(const char* dirPath, const std::string& fileName,
                      Json& fileData)
{
    int rc = 0;

    fs::path filePath(dirPath);
    filePath /= fileName;

    std::ifstream jsonFile(filePath);
    if (!jsonFile.is_open())
    {
        log<level::ERR>("BIOS config file does not exist",
                        entry("FILE=%s", filePath.c_str()));
        rc = -1;
    }
    else
    {
        fileData = Json::parse(jsonFile, nullptr, false);
        if (fileData.is_discarded())
        {
            log<level::ERR>("Parsing config file failed",
                            entry("FILE=%s", filePath.c_str()));
            rc = -1;
        }
    }

    return rc;
}

namespace bios_enum
{

namespace internal
{

using PropertyValue =
    std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t,
                 uint64_t, double, std::string>;
using Value = std::string;

/** @struct DBusMapping
 *
 *  Data structure for storing information regarding BIOS enumeration attribute
 *  and the D-Bus object for the attribute.
 */
struct DBusMapping
{
    std::string objectPath;   //!< D-Bus object path
    std::string interface;    //!< D-Bus interface
    std::string propertyName; //!< D-Bus property name
    std::map<PropertyValue, Value>
        dBusValToValMap; //!< Map of D-Bus property
                         //!< value to attribute value
};

/** @brief Map containing the possible and the default values for the BIOS
 *         enumeration type attributes.
 */
AttrValuesMap valueMap;

/** @brief Map containing the optional D-Bus property information about the
 *         BIOS enumeration type attributes.
 */
std::map<AttrName, std::optional<DBusMapping>> attrLookup;

/** @brief Populate the mapping between D-Bus property value and attribute value
 *         for the BIOS enumeration attribute.
 *
 *  @param[in] type - type of the D-Bus property
 *  @param[in] dBusValues - json array of D-Bus property values
 *  @param[in] pv - Possible values for the BIOS enumeration attribute
 *  @param[out] mapping - D-Bus mapping object for the attribute
 *
 */
void populateMapping(const std::string& type, const Json& dBusValues,
                     const PossibleValues& pv, DBusMapping& mapping)
{
    size_t pos = 0;
    PropertyValue value;
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
            log<level::ERR>("Unknown D-Bus property type",
                            entry("TYPE=%s", type.c_str()));
        }

        mapping.dBusValToValMap.emplace(value, pv[pos]);
    }
}

/** @brief Read the possible values for the BIOS enumeration type
 *
 *  @param[in] possibleValues - json array of BIOS enumeration possible values
 */
PossibleValues readPossibleValues(Json& possibleValues)
{
    Strings biosStrings{};

    for (auto& val : possibleValues)
    {
        biosStrings.emplace_back(std::move(val));
    }

    return biosStrings;
}

} // namespace internal

int setupValueLookup(const char* dirPath)
{
    int rc = 0;

    if (!internal::valueMap.empty() && !internal::attrLookup.empty())
    {
        return rc;
    }

    // Parse the BIOS enumeration config file
    fs::path filePath(dirPath);
    filePath /= bIOSEnumJson;

    std::ifstream jsonFile(filePath);
    if (!jsonFile.is_open())
    {
        log<level::ERR>("BIOS enum config file does not exist",
                        entry("FILE=%s", filePath.c_str()));
        rc = -1;
        return rc;
    }

    auto fileData = Json::parse(jsonFile, nullptr, false);
    if (fileData.is_discarded())
    {
        log<level::ERR>("Parsing config file failed");
        rc = -1;
        return rc;
    }

    auto entries = fileData.value("entries", emptyJsonList);
    // Iterate through each JSON object in the config file
    for (const auto& entry : entries)
    {
        std::string attr = entry.value("attribute_name", "");
        PossibleValues possibleValues;
        DefaultValues defaultValues;

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

        std::optional<internal::DBusMapping> dBusMap = std::nullopt;

        if (entry.count("dbus") != 0)
        {
            auto dBusEntry = entry.value("dbus", emptyJson);
            dBusMap = std::make_optional<internal::DBusMapping>();
            dBusMap.value().objectPath = dBusEntry.value("object_path", "");
            dBusMap.value().interface = dBusEntry.value("interface", "");
            dBusMap.value().propertyName = dBusEntry.value("property_name", "");
            std::string propType = dBusEntry.value("property_type", "");
            Json propValues = dBusEntry["property_values"];
            internal::populateMapping(propType, propValues, possibleValues,
                                      dBusMap.value());
        }

        internal::attrLookup.emplace(attr, std::move(dBusMap));

        // Defaulting all the types of attributes to BIOSEnumeration
        internal::valueMap.emplace(
            std::move(attr), std::make_tuple(false, std::move(possibleValues),
                                             std::move(defaultValues)));
    }

    return rc;
}

const AttrValuesMap& getValues()
{
    return internal::valueMap;
}

CurrentValues getAttrValue(const AttrName& attrName)
{
    const auto& dBusMap = internal::attrLookup.at(attrName);
    CurrentValues currentValues;
    internal::PropertyValue propValue;

    if (dBusMap == std::nullopt)
    {
        const auto& valueEntry = internal::valueMap.at(attrName);
        const auto& [readOnly, possibleValues, currentValues] = valueEntry;
        return currentValues;
    }

    auto bus = sdbusplus::bus::new_default();
    auto service = pldm::responder::getService(bus, dBusMap.value().objectPath,
                                               dBusMap.value().interface);
    auto method =
        bus.new_method_call(service.c_str(), dBusMap.value().objectPath.c_str(),
                            "org.freedesktop.DBus.Properties", "Get");
    method.append(dBusMap.value().interface, dBusMap.value().propertyName);
    auto reply = bus.call(method);
    reply.read(propValue);

    auto iter = dBusMap.value().dBusValToValMap.find(propValue);
    if (iter != dBusMap.value().dBusValToValMap.end())
    {
        currentValues.push_back(iter->second);
    }

    return currentValues;
}

} // namespace bios_enum

namespace bios_string
{

/** @brief BIOS string types
 */
enum BiosStringEncoding
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

/** @struct DBusMapping
 *
 *  Data structure for storing information regarding BIOS string attribute
 *  and the D-Bus object for the attribute.
 */
struct DBusMapping
{
    std::string objectPath;   //!< D-Bus object path
    std::string interface;    //!< D-Bus interface
    std::string propertyName; //!< D-Bus property name
};

/** @brief Map containing the possible and the default values for the BIOS
 *         string type attributes.
 */
AttrValuesMap valueMap;

/** @brief Map containing the optional D-Bus property information about the
 *         BIOS string type attributes.
 */
std::map<AttrName, std::optional<DBusMapping>> attrLookup;

} // namespace internal

int setupValueLookup(const char* dirPath)
{
    int rc = 0;

    if (!internal::valueMap.empty() && !internal::attrLookup.empty())
    {
        return rc;
    }

    Json fileData;
    rc = parseBiosJsonFile(dirPath, bIOSStrJson, fileData);
    if (rc != 0)
    {
        return rc;
    }

    auto entries = fileData.value("entries", emptyJsonList);
    // Iterate through each JSON object in the config file
    for (const auto& entry : entries)
    {
        std::string attr = entry.value("attribute_name", "");
        // Transfer string type from string to enum
        std::string strTypeTmp = entry.value("string_type", "Unknown");
        auto iter = strTypeMap.find(strTypeTmp);
        if (iter == strTypeMap.end())
        {
            log<level::ERR>(
                "Wrong string type",
                phosphor::logging::entry("STRING_TYPE=%s", strTypeTmp.c_str()),
                phosphor::logging::entry("ATTRIBUTE_NAME=%s", attr.c_str()));
            return -1;
        }
        uint8_t strType = iter->second;

        uint16_t minStrLen = entry.value("minimum_string_length", 0);
        uint16_t maxStrLen = entry.value("maximum_string_length", 0);
        if (maxStrLen - minStrLen < 0)
        {
            log<level::ERR>(
                "Maximum string length is smaller than minimum string length",
                phosphor::logging::entry("ATTRIBUTE_NAME=%s", attr.c_str()));
            return -1;
        }
        uint16_t defaultStrLen = entry.value("default_string_length", 0);
        std::string defaultStr = entry.value("default_string", "");
        if ((defaultStrLen == 0) && (defaultStr.size() > 0))
        {
            log<level::ERR>(
                "Default string length is 0, but default string is existing",
                phosphor::logging::entry("ATTRIBUTE_NAME=%s", attr.c_str()));
            return -1;
        }

        // dbus handling
        std::optional<internal::DBusMapping> dBusMap;

        if (entry.count("dbus") != 0)
        {
            auto dBusEntry = entry.value("dbus", emptyJson);
            dBusMap = std::make_optional<internal::DBusMapping>();
            dBusMap->objectPath = dBusEntry.value("object_path", "");
            dBusMap->interface = dBusEntry.value("interface", "");
            dBusMap->propertyName = dBusEntry.value("property_name", "");
            if (dBusMap->objectPath.empty() || dBusMap->interface.empty() ||
                dBusMap->propertyName.empty())
            {
                log<level::ERR>(
                    "Invalid dbus config",
                    phosphor::logging::entry("OBJPATH=%s",
                                             dBusMap->objectPath.c_str()),
                    phosphor::logging::entry("INTERFACE=%s",
                                             dBusMap->interface.c_str()),
                    phosphor::logging::entry("PROPERTY_NAME=%s",
                                             dBusMap->propertyName.c_str()));
                return -1;
            }
        }

        internal::attrLookup.emplace(attr, std::move(dBusMap));

        // Defaulting all the types of attributes to BIOSString
        internal::valueMap.emplace(
            std::move(attr),
            std::make_tuple(entry.count("dbus") == 0, strType, minStrLen,
                            maxStrLen, defaultStrLen, std::move(defaultStr)));
    }

    return rc;
}

const AttrValuesMap& getValues()
{
    return internal::valueMap;
}
} // namespace bios_string

Strings getStrings(const char* dirPath)
{
    Strings biosStrings{};
    fs::path dir(dirPath);

    if (!fs::exists(dir) || fs::is_empty(dir))
    {
        return biosStrings;
    }

    for (const auto& file : fs::directory_iterator(dir))
    {
        std::ifstream jsonFile(file.path().c_str());
        if (!jsonFile.is_open())
        {
            log<level::ERR>("JSON BIOS config file does not exist",
                            entry("FILE=%s", file.path().filename().c_str()));
            continue;
        }

        auto fileData = Json::parse(jsonFile, nullptr, false);
        if (fileData.is_discarded())
        {
            log<level::ERR>("Parsing config file failed",
                            entry("FILE=%s", file.path().filename().c_str()));
            continue;
        }

        auto entries = fileData.value("entries", emptyJsonList);

        // Iterate through each entry in the config file
        for (auto& entry : entries)
        {
            biosStrings.emplace_back(entry.value("attribute_name", ""));

            // For BIOS enumeration attributes the possible values are strings
            if (file.path().filename() == bIOSEnumJson)
            {
                auto possibleValues = bios_enum::internal::readPossibleValues(
                    entry["possible_values"]);
                std::move(possibleValues.begin(), possibleValues.end(),
                          std::back_inserter(biosStrings));
            }
        }
    }

    return biosStrings;
}

} // namespace bios_parser
