#include "bios_enum_attribute.hpp"

#include "utils.hpp"

#include <iostream>

namespace pldm
{
namespace responder
{
namespace bios
{
namespace wip
{

BIOSEnumAttribute::BIOSEnumAttribute(const Json& entry,
                                     const BIOSStringTable& stringTable) :
    BIOSAttribute(entry)
{
    std::string attrName = entry.at("attribute_name");
    Json pv = entry.at("possible_values");
    for (auto& val : pv)
    {
        possibleValues.emplace_back(val);
    }

    std::vector<std::string> defaultValues;
    Json dv = entry.at("default_values");
    for (auto& val : dv)
    {
        defaultValues.emplace_back(val);
    }
    assert(defaultValues.size() == 1);
    defaultValue = defaultValues[0];

    buildEntryInfo(stringTable);
    if (!readOnly)
    {
        auto dbusValues = entry.at("dbus").at("property_values");
        buildValMap(dbusValues);
    }
}

std::vector<uint8_t> BIOSEnumAttribute::findValueIndex(const std::string value)
{
    auto iter = std::find_if(possibleValues.begin(), possibleValues.end(),
                             [&value](const auto& v) { return v == value; });
    if (iter == possibleValues.end())
    {
        throw std::invalid_argument("value must be one of possible value");
    }
    uint8_t defIndex = iter - possibleValues.begin();
    return {defIndex};
}

void BIOSEnumAttribute::buildEntryInfo(const BIOSStringTable& stringTable)
{
    defaultIndices = findValueIndex(defaultValue);

    for (const auto& pv : possibleValues)
    {
        auto handle = stringTable.findHandle(pv);
        possibleValuesHandle.push_back(handle);
    }

    enumEntryInfo = {
        stringTable.findHandle(name),         readOnly,
        (uint8_t)possibleValuesHandle.size(), possibleValuesHandle.data(),
        (uint8_t)defaultIndices.size(),       defaultIndices.data()};
}

void BIOSEnumAttribute::buildValMap(const Json& dbusVals)
{
    PropertyValue value;
    size_t pos = 0;
    for (auto it = dbusVals.begin(); it != dbusVals.end(); ++it, ++pos)
    {
        if (dBusMap->propertyType == "uint8_t")
        {
            value = static_cast<uint8_t>(it.value());
        }
        else if (dBusMap->propertyType == "uint16_t")
        {
            value = static_cast<uint16_t>(it.value());
        }
        else if (dBusMap->propertyType == "uint32_t")
        {
            value = static_cast<uint32_t>(it.value());
        }
        else if (dBusMap->propertyType == "uint64_t")
        {
            value = static_cast<uint64_t>(it.value());
        }
        else if (dBusMap->propertyType == "int16_t")
        {
            value = static_cast<int16_t>(it.value());
        }
        else if (dBusMap->propertyType == "int32_t")
        {
            value = static_cast<int32_t>(it.value());
        }
        else if (dBusMap->propertyType == "int64_t")
        {
            value = static_cast<int64_t>(it.value());
        }
        else if (dBusMap->propertyType == "bool")
        {
            value = static_cast<bool>(it.value());
        }
        else if (dBusMap->propertyType == "double")
        {
            value = static_cast<double>(it.value());
        }
        else if (dBusMap->propertyType == "string")
        {
            value = static_cast<std::string>(it.value());
        }
        else
        {
            std::cerr << "Unknown D-Bus property type, TYPE="
                      << dBusMap->propertyType << "\n";
            throw std::invalid_argument("Unknown D-BUS property type");
        }
        valMap.emplace(value, possibleValues[pos]);
    }
}

std::vector<uint8_t> BIOSEnumAttribute::getAttrValue()
{
    try
    {
        if (readOnly)
        {
            return defaultIndices;
        }
        auto propValue =
            pldm::utils::DBusHandler().getDbusPropertyVariant<PropertyValue>(
                dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
                dBusMap->interface.c_str());
        auto iter = valMap.find(propValue);
        if (iter == valMap.end())
        {
            return defaultIndices;
        }

        auto currentValue = iter->second;
        return findValueIndex(currentValue);
    }
    catch (const std::exception& e)
    {
        return defaultIndices;
    }
}

void BIOSEnumAttribute::updateDbusProperty(const PropertyValue& value)
{
    auto setDbusProperty = [this](const auto& variant) {
        pldm::utils::DBusHandler().setDbusProperty(
            dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
            dBusMap->interface.c_str(), variant);
    };

    if (dBusMap->propertyType == "uint8_t")
    {
        std::variant<uint8_t> v = std::get<uint8_t>(value);
        setDbusProperty(v);
    }
    else if (dBusMap->propertyType == "int16_t")
    {
        std::variant<int16_t> v = std::get<int16_t>(value);
        setDbusProperty(v);
    }
    else if (dBusMap->propertyType == "uint16_t")
    {
        std::variant<uint16_t> v = std::get<uint16_t>(value);
        setDbusProperty(v);
    }
    else if (dBusMap->propertyType == "int32_t")
    {
        std::variant<int32_t> v = std::get<int32_t>(value);
        setDbusProperty(v);
    }
    else if (dBusMap->propertyType == "uint32_t")
    {
        std::variant<uint32_t> v = std::get<uint32_t>(value);
        setDbusProperty(v);
    }
    else if (dBusMap->propertyType == "int64_t")
    {
        std::variant<int64_t> v = std::get<int64_t>(value);
        setDbusProperty(v);
    }
    else if (dBusMap->propertyType == "uint64_t")
    {
        std::variant<uint64_t> v = std::get<uint64_t>(value);
        setDbusProperty(v);
    }
    else if (dBusMap->propertyType == "double")
    {
        std::variant<double> v = std::get<double>(value);
        setDbusProperty(v);
    }
    else if (dBusMap->propertyType == "string")
    {
        std::variant<std::string> v = std::get<std::string>(value);
        setDbusProperty(v);
    }
    else
    {
        assert(false && "UnSpported Dbus Type");
    }
}

void BIOSEnumAttribute::setAttrValueOnDbus(
    const pldm_bios_attr_val_table_entry* attrValueEntry,
    const pldm_bios_attr_table_entry* attrEntry,
    const BIOSStringTable& stringTable)
{
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

    auto it = std::find_if(valMap.begin(), valMap.end(),
                           [&valueString](const auto& typePair) {
                               return typePair.second == valueString;
                           });
    if (it == valMap.end())
    {
        throw std::invalid_argument("Invalid Enum Value");
    }

    updateDbusProperty(it->first);
}

void BIOSEnumAttribute::constructEntry(const BIOSStringTable&, Table& attrTable,
                                       Table& attrValueTable)
{
    auto entryLength = pldm_bios_table_attr_entry_enum_encode_length(
        possibleValuesHandle.size(), defaultIndices.size());

    auto tableSize = attrTable.size();
    attrTable.resize(tableSize + entryLength, 0);

    pldm_bios_table_attr_entry_enum_encode(attrTable.data() + tableSize,
                                           entryLength, &enumEntryInfo);

    auto attrTableEntry = reinterpret_cast<pldm_bios_attr_table_entry*>(
        attrTable.data() + tableSize);
    auto attrHandle =
        pldm_bios_table_attr_entry_decode_attribute_handle(attrTableEntry);
    auto attrType =
        pldm_bios_table_attr_entry_decode_attribute_type(attrTableEntry);

    auto currValueIndices = getAttrValue();

    entryLength = pldm_bios_table_attr_value_entry_encode_enum_length(
        currValueIndices.size());
    tableSize = attrValueTable.size();
    attrValueTable.resize(tableSize + entryLength);
    pldm_bios_table_attr_value_entry_encode_enum(
        attrValueTable.data() + tableSize, entryLength, attrHandle, attrType,
        currValueIndices.size(), currValueIndices.data());
}

} // namespace wip
} // namespace bios
} // namespace responder
} // namespace pldm