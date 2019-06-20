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

Strings getStrings(const char* dirPath)
{
    Strings biosStrings{};
    fs::path dir(dirPath);

    if (!fs::exists(dir) || fs::is_empty(dir))
    {
        return biosStrings;
    }

    for (auto& file : fs::directory_iterator(dir))
    {
        std::ifstream jsonFile(file.path().c_str());

        if (!jsonFile.is_open())
        {
            log<level::ERR>("JSON BIOS config file does not exist",
                            entry("FILE=%s", file.path().filename().c_str()));
            return biosStrings;
        }

        auto fileData = Json::parse(jsonFile, nullptr, false);
        if (fileData.is_discarded())
        {
            log<level::ERR>("Parsing config file failed");
            return biosStrings;
        }

        static const std::vector<Json> emptyList{};
        auto entries = fileData.value("entries", emptyList);
        for (const auto& entry : entries)
        {
            // Iterate through each JSON object in the config file
            biosStrings.emplace_back(entry.value("attribute_name", ""));

            Json pv = entry["possible_values"];
            for (const auto& val : pv)
            {
                if (val.type() == Json::value_t::string)
                {
                    biosStrings.emplace_back(val);
                }
            }
        }
    }
    return biosStrings;
}

namespace bios_enum
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
        valToDBusValMap; //!< Map of D-Bus property
                         //!< value to attribute value
};

namespace internal
{

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

        mapping.valToDBusValMap.emplace(value, pv[pos]);
    }
}

} // namespace internal

AttrValuesMap getValues(const char* dirPath)
{
    constexpr auto bIOSEnumJson = "enum_attrs.json";
    AttrValuesMap valueMap{};
    fs::path filePath(dirPath);
    filePath /= bIOSEnumJson;

    std::ifstream jsonFile(filePath);
    if (!jsonFile.is_open())
    {
        log<level::ERR>("BIOS enum config file does not exist",
                        entry("FILE=%s", filePath.c_str()));
        return valueMap;
    }

    auto fileData = Json::parse(jsonFile, nullptr, false);
    if (fileData.is_discarded())
    {
        log<level::ERR>("Parsing config file failed");
        return valueMap;
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
        valueMap.emplace(attr,
                         std::make_tuple(false, possibleValues, defaultValues));

        std::optional<DBusMapping> dBusmap = std::nullopt;
        if (entry.count("object_path") != 0)
        {
            dBusmap = std::make_optional<DBusMapping>();
            dBusmap.value().objectPath = entry.value("object_path", "");
            dBusmap.value().interface = entry.value("interface", "");
            dBusmap.value().propertyName = entry.value("property_name", "");
            std::string propType = entry.value("property_type", "");
            Json propValues = entry["property_values"];
            internal::populateMapping(propType, propValues, possibleValues,
                                      dBusmap.value());
        }

        internal::attrLookup.emplace(attr,
                                     std::make_tuple(defaultValues, dBusmap));
    }

    return valueMap;
}

CurrentValues getAttrValue(const std::string& attrName)
{
    auto entry = internal::attrLookup.at(attrName);
    const auto& [cv, dBusMap] = entry;
    CurrentValues currentValues;
    PropertyValue propValue;

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

    auto iter = dBusMap.value().valToDBusValMap.find(propValue);
    if (iter != dBusMap.value().valToDBusValMap.end())
    {
        currentValues.push_back(iter->second);
    }

    return currentValues;
}

} // namespace bios_enum

} // namespace bios_parser
