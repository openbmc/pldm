#include "bios_parser.hpp"

#include "libpldmresponder/utils.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <phosphor-logging/log.hpp>

namespace bios_parser
{

using Json = nlohmann::json;
namespace fs = std::filesystem;
using namespace phosphor::logging;
constexpr auto bIOSEnumJson = "enum_attrs.json";

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
        DBusValToValMap; //!< Map of D-Bus property
                         //!< value to attribute value
};

AttrValuesMap valueMap;

std::map<AttrName, std::tuple<DefaultValues, std::optional<DBusMapping>>>
    attrLookup;

/** @brief Populate the mapping between D-Bus property value and attribute value
 *         for the BIOS enumeration attribute.
 *
 * @param[in] type - type of the D-Bus property
 * @param[in] json - json array of D-Bus property values
 * @param[in] pv - Possible values for the BIOS enumeration attribute
 * @param[out] mapping - D-Bus mapping object for the attribute
 *
 */
void populateMapping(const std::string& type, const Json& json,
                     const PossibleValues& pv, DBusMapping& mapping)
{
    size_t pos = 0;
    for (auto it = json.begin(); it != json.end(); ++it, ++pos)
    {
        PropertyValue value;
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
        else
        {
            value = static_cast<std::string>(it.value());
        }

        mapping.DBusValToValMap.emplace(value, pv[pos]);
    }
}

PossibleValues readPossibleValues(const Json& json)
{
    Strings biosStrings{};

    for (const auto& val : json)
    {
        biosStrings.emplace_back(val);
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

    static const std::vector<Json> emptyList{};
    auto entries = fileData.value("entries", emptyList);
    // Iterate through each JSON object in the config file
    for (const auto& entry : entries)
    {
        std::string attr = entry.value("attribute_name", "");
        PossibleValues possibleValues;
        DefaultValues defaultValues;

        Json pv = entry["possible_values"];
        for (const auto& val : pv)
        {
            possibleValues.emplace_back(val);
        }

        Json dv = entry["default_values"];
        for (const auto& val : dv)
        {
            defaultValues.emplace_back(val);
        }

        // Defaulting all the types of attributes to BIOSEnumeration
        internal::valueMap.emplace(
            attr, std::make_tuple(false, possibleValues, defaultValues));

        std::optional<internal::DBusMapping> dBusMap = std::nullopt;
        static const Json empty{};
        if (entry.count("dbus") != 0)
        {
            auto dBusEntry = entry.value("dbus", empty);
            dBusMap = std::make_optional<internal::DBusMapping>();
            dBusMap.value().objectPath = dBusEntry.value("object_path", "");
            dBusMap.value().interface = dBusEntry.value("interface", "");
            dBusMap.value().propertyName = dBusEntry.value("property_name", "");
            std::string propType = dBusEntry.value("property_type", "");
            Json propValues = dBusEntry["property_values"];
            internal::populateMapping(propType, propValues, possibleValues,
                                      dBusMap.value());
        }

        internal::attrLookup.emplace(
            attr, std::make_tuple(std::move(defaultValues), dBusMap));
    }

    return rc;
}

AttrValuesMap getValues()
{
    return internal::valueMap;
}

CurrentValues getAttrValue(const std::string& attrName)
{
    auto entry = internal::attrLookup.at(attrName);
    const auto& [cv, dBusMap] = entry;
    CurrentValues currentValues;
    internal::PropertyValue propValue;

    if (dBusMap == std::nullopt)
    {
        return cv;
    }

    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto service = pldm::responder::getService(
            bus, dBusMap.value().objectPath, dBusMap.value().interface);
        auto method = bus.new_method_call(
            service.c_str(), dBusMap.value().objectPath.c_str(),
            "org.freedesktop.DBus.Properties", "Get");
        method.append(dBusMap.value().interface, dBusMap.value().propertyName);
        auto reply = bus.call(method);
        reply.read(propValue);
    }
    catch (std::exception& e)
    {
        log<level::ERR>("Error getting property");
        return cv;
    }

    auto iter = dBusMap.value().DBusValToValMap.find(propValue);
    if (iter != dBusMap.value().DBusValToValMap.end())
    {
        currentValues.push_back(iter->second);
    }

    return currentValues;
}

} // namespace bios_enum

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
            log<level::ERR>("Parsing config file failed");
            continue;
        }

        static const std::vector<Json> emptyList{};
        auto entries = fileData.value("entries", emptyList);

        // Iterate through each entry in the config file
        for (const auto& entry : entries)
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
