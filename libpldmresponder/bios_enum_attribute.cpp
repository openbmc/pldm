#include "config.h"

#include "bios_enum_attribute.hpp"

#include "utils.hpp"

#include <iostream>

namespace pldm
{
namespace responder
{
namespace bios
{

BIOSEnumAttribute::BIOSEnumAttribute(const Json& entry,
                                     const BIOSStringTable& stringTable,
                                     DBusHandler* const dbusHandler) :
    BIOSAttribute(entry, stringTable, dbusHandler)
{
    std::string attrName = entry.at("attribute_name");
    Json pv = entry.at("possible_values");
    for (auto& val : pv)
    {
        possibleValues.emplace_back(val);
    }

    for (auto str : possibleValues)
    {
        strings.emplace(str, stringTable.findHandle(str));
    }

    std::vector<std::string> defaultValues;
    Json dv = entry.at("default_values");
    for (auto& val : dv)
    {
        defaultValues.emplace_back(val);
    }
    assert(defaultValues.size() == 1);
    defaultValue = defaultValues[0];
    if (!readOnly)
    {
        auto dbusValues = entry.at("dbus").at("property_values");
        buildValMap(dbusValues);
    }

    attrType =
        readOnly ? PLDM_BIOS_ENUMERATION_READ_ONLY : PLDM_BIOS_ENUMERATION;
}

uint8_t BIOSEnumAttribute::getValueIndex(const std::string& value)
{
    auto iter = std::find_if(possibleValues.begin(), possibleValues.end(),
                             [&value](const auto& v) { return v == value; });
    if (iter == possibleValues.end())
    {
        throw std::invalid_argument("value must be one of possible value");
    }
    return iter - possibleValues.begin();
}

std::vector<uint16_t> BIOSEnumAttribute::getPossibleValuesHandle()
{
    std::vector<uint16_t> possibleValuesHandle;
    for (const auto& pv : possibleValues)
    {
        auto handle = strings[pv];
        possibleValuesHandle.push_back(handle);
    }

    return possibleValuesHandle;
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

uint8_t BIOSEnumAttribute::getAttrValueIndex()
{
    auto defaultValueIndex = getValueIndex(defaultValue);
    if (readOnly)
    {
        return defaultValueIndex;
    }

    try
    {
        auto propValue = dbusHandler->getDbusPropertyVariant(
            dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
            dBusMap->interface.c_str());
        auto iter = valMap.find(propValue);
        if (iter == valMap.end())
        {
            return defaultValueIndex;
        }
        auto currentValue = iter->second;
        return getValueIndex(currentValue);
    }
    catch (const std::exception& e)
    {
        return defaultValueIndex;
    }
}

void BIOSEnumAttribute::setAttrValueOnDbus(
    const pldm_bios_attr_val_table_entry* attrValueEntry)
{
    if (readOnly)
    {
        return;
    }
    auto currHdls = table::attribute_value::decodeEnumEntry(attrValueEntry);

    assert(currHdls.size() == 1);

    auto valueString = possibleValues[currHdls[0]];

    auto it = std::find_if(valMap.begin(), valMap.end(),
                           [&valueString](const auto& typePair) {
                               return typePair.second == valueString;
                           });
    if (it == valMap.end())
    {
        return;
    }

    dbusHandler->setDbusProperty(*dBusMap, it->first);
}

void BIOSEnumAttribute::constructEntry(Table& attrTable, Table& attrValueTable)
{
    auto possibleValuesHandle = getPossibleValuesHandle();
    std::vector<uint8_t> defaultIndices(1, 0);
    defaultIndices[0] = getValueIndex(defaultValue);

    pldm_bios_table_attr_entry_enum_info info = {
        attrHandle,
        attrNameHandle,
        attrType,
        (uint8_t)possibleValuesHandle.size(),
        possibleValuesHandle.data(),
        (uint8_t)defaultIndices.size(),
        defaultIndices.data(),
    };

    table::attribute::constructEnumEntry(attrTable, &info);

    std::vector<uint8_t> currValueIndices(1, 0);
    currValueIndices[0] = getAttrValueIndex();

    table::attribute_value::constructEnumEntry(attrValueTable, attrHandle,
                                               attrType, currValueIndices);
}

int BIOSEnumAttribute::updateAttrVal(Table& newValue,
                                     const PropertyValue& newPropVal)
{
    auto iter = valMap.find(newPropVal);
    if (iter == valMap.end())
    {
        std::cerr << "Could not find index for new BIOS enum, value="
                  << std::get<std::string>(newPropVal) << "\n";
        return PLDM_ERROR;
    }
    auto currentValue = iter->second;
    std::vector<uint8_t> handleIndices{getValueIndex(currentValue)};
    table::attribute_value::constructEnumEntry(newValue, attrHandle, attrType,
                                               handleIndices);
    return PLDM_SUCCESS;
}

} // namespace bios
} // namespace responder
} // namespace pldm
