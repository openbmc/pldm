#include "bios_boot_config_setting_attribute.hpp"

#include "common/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>

PHOSPHOR_LOG2_USING;

namespace pldm::responder::bios
{
BIOSBootConfigSettingAttribute::BIOSBootConfigSettingAttribute(
    const Json& entry, pldm::utils::DBusHandler* const dbusHandler) :
    BIOSAttribute(entry, dbusHandler)
{
    Json childentry = entry.at("possible_boot_source_settings");
    for (auto& possibleSetting : childentry)
    {
        possibleBootSources.emplace_back(possibleSetting);
    }

    childentry = entry.at("default_boot_source_settings");
    for (auto& defaultSetting : childentry)
    {
        defaultBootSources.emplace_back(defaultSetting);
    }

    childentry = entry.at("value_names");
    for (auto& valueName : childentry)
    {
        valueDisplayNames.emplace_back(valueName);
    }

    const std::string bootConfigType = entry.at("boot_config_type");
    if (!bootConfigTranslator.contains(bootConfigType))
    {
        error("{FUNC}: Invalid BootConfigType={TYPE}", "FUNC",
              std::string(__func__), "TYPE", bootConfigType);
        throw std::invalid_argument("Invalid BootConfigType");
    }
    bootConfigSettingInfo.bootConfigType =
        static_cast<uint8_t>(bootConfigTranslator.at(bootConfigType));

    const uint8_t minBootSourceCount =
        entry.at("minimum_number_of_boot_sources_settings");
    bootConfigSettingInfo.minimumBootSourceCount = minBootSourceCount;

    const uint8_t maxBootSourceCount =
        entry.at("maximum_number_of_boot_sources_settings");
    bootConfigSettingInfo.maximumBootSourceCount = maxBootSourceCount;

    const std::string supportedModes =
        entry.at("supported_ordered_and_fail_through_modes");
    if (!supportedOrderedAndFailThroughModesTranslator.contains(supportedModes))
    {
        error("{FUNC}: Invalid SupportedOrderedAndFailThroughModes={TYPE}",
              "FUNC", std::string(__func__), "TYPE", supportedModes);
        throw std::invalid_argument(
            "Invalid SupportedOrderedAndFailThroughModes");
    }
    bootConfigSettingInfo.supportedOrderedAndFailThoughModes =
        static_cast<uint8_t>(
            supportedOrderedAndFailThroughModesTranslator.at(supportedModes));

    if (dBusMap.has_value())
    {
        auto dbusValues = entry.at("dbus").at("property_values");
        buildValMap(dbusValues);
    }
}

void BIOSBootConfigSettingAttribute::setAttrValueOnDbus(
    const pldm_bios_attr_val_table_entry* attrValueEntry,
    const pldm_bios_attr_table_entry* attrEntry,
    const BIOSStringTable& stringTable)
{
    if (!dBusMap.has_value())
    {
        return;
    }
    auto possibleHdls =
        table::attribute::decodeBootConfigSettingEntry(attrEntry)
            .possibleSettingStringHandles;
    auto currHdls =
        table::attribute_value::decodeBootConfigSettingEntry(attrValueEntry);

    std::vector<std::string> newVals{};
    for (const auto handle : currHdls)
    {
        auto valueString = stringTable.findString(possibleHdls[handle]);

        auto it = std::find_if(valMap.begin(), valMap.end(),
                               [&valueString](const auto& typePair) {
            return typePair.second == valueString;
        });
        if (it != valMap.end())
        {
            newVals.emplace_back(std::get<std::string>(it->first));
        }
        else
        {
            error("Could not find value for handle={HANDLE}", "HANDLE",
                  possibleHdls[handle]);
            return;
        }
    }

    dbusHandler->setDbusProperty(*dBusMap, pldm::utils::PropertyValue{newVals});
}

int BIOSBootConfigSettingAttribute::updateAttrVal(
    Table& newValue, uint16_t attrHdl, uint8_t attrType,
    const pldm::utils::PropertyValue& newPropVal)
{
    auto newBootSettings = std::get<std::vector<std::string>>(newPropVal);
    std::vector<uint8_t> newHandleIndices{};

    for (const auto& bootSetting : newBootSettings)
    {
        if (!valMap.contains(bootSetting))
        {
            error("Could not find index for new BIOS Boot, value={PROP_VAL}",
                  "PROP_VAL", std::get<std::string>(newPropVal));
            return PLDM_ERROR;
        }

        newHandleIndices.emplace_back(
            getValueIndex(valMap.at(bootSetting), possibleBootSources));
    }

    table::attribute_value::constructBootConfigSettingEntry(
        newValue, attrHdl, attrType, bootConfigSettingInfo.bootConfigType,
        bootConfigSettingInfo.supportedOrderedAndFailThoughModes,
        newHandleIndices);

    return PLDM_SUCCESS;
}

void BIOSBootConfigSettingAttribute::populateValueDisplayNamesMap(
    uint16_t attrHandle)
{
    for (auto& vdn : valueDisplayNames)
    {
        valueDisplayNamesMap[attrHandle].push_back(vdn);
    }
}

void BIOSBootConfigSettingAttribute::constructEntry(
    const BIOSStringTable& stringTable, Table& attrTable, Table& attrValueTable,
    std::optional<std::variant<int64_t, std::string, std::vector<std::string>>>
        optAttributeValue)
{
    auto possibleValuesHandles = getPossibleValuesHandles(stringTable,
                                                          possibleBootSources);

    std::vector<uint8_t> defaultIndices = getDefaultIndices();

    pldm_bios_table_attr_entry_boot_config_setting_info info = {
        stringTable.findHandle(name),
        readOnly,
        bootConfigSettingInfo.bootConfigType,
        bootConfigSettingInfo.supportedOrderedAndFailThoughModes,
        bootConfigSettingInfo.minimumBootSourceCount,
        bootConfigSettingInfo.maximumBootSourceCount,
        (uint8_t)possibleValuesHandles.size(),
        possibleValuesHandles.data()};

    auto attrTableEntry =
        table::attribute::constructBootConfigSettingEntry(attrTable, &info);

    auto [attrHandle, attrType,
          _] = table::attribute::decodeHeader(attrTableEntry);

    populateValueDisplayNamesMap(attrHandle);

    std::vector<uint8_t> currValueIndices{};
    if (optAttributeValue.has_value())
    {
        auto attributeValue = optAttributeValue.value();
        if (attributeValue.index() == 2)
        {
            auto currValues =
                std::get<std::vector<std::string>>(attributeValue);
            for (const auto& currValue : currValues)
            {
                currValueIndices.emplace_back(
                    getValueIndex(currValue, possibleBootSources));
            }
        }
        else
        {
            currValueIndices = defaultIndices;
        }
    }
    else
    {
        currValueIndices = defaultIndices;
    }

    table::attribute_value::constructBootConfigSettingEntry(
        attrValueTable, attrHandle, attrType,
        bootConfigSettingInfo.bootConfigType,
        bootConfigSettingInfo.supportedOrderedAndFailThoughModes,
        currValueIndices);
}

void BIOSBootConfigSettingAttribute::generateAttributeEntry(
    const std::variant<int64_t, std::string,
                       std::vector<std::string>>& /*attributevalue*/,
    Table& /*attrValueEntry*/)
{
    // TODO: Support runtime update of BIOS Boot Config Setting attributes.
    return;
}

std::vector<uint16_t> BIOSBootConfigSettingAttribute::getPossibleValuesHandles(
    const BIOSStringTable& stringTable,
    const std::vector<std::string>& possibleBootSources)
{
    std::vector<uint16_t> possibleValuesHandle{};
    for (const auto& possibleValue : possibleBootSources)
    {
        auto handle = stringTable.findHandle(possibleValue);
        possibleValuesHandle.push_back(handle);
    }

    return possibleValuesHandle;
}

std::vector<uint8_t> BIOSBootConfigSettingAttribute::getDefaultIndices()
{
    std::vector<uint8_t> defaultIndices{};
    for (const auto& defaultBootSource : defaultBootSources)
    {
        auto index = getValueIndex(defaultBootSource, possibleBootSources);
        defaultIndices.push_back(index);
    }

    return defaultIndices;
}

uint8_t BIOSBootConfigSettingAttribute::getValueIndex(
    const std::string& value, const std::vector<std::string>& possibleValues)
{
    auto iter = std::find_if(possibleValues.begin(), possibleValues.end(),
                             [&value](const auto& v) { return v == value; });
    if (iter == possibleValues.end())
    {
        throw std::invalid_argument("value must be one of possible value");
    }
    return iter - possibleValues.begin();
}

void BIOSBootConfigSettingAttribute::buildValMap(const Json& dbusVals)
{
    pldm::utils::PropertyValue value;
    size_t index;

    if (dBusMap->propertyType != "stringArray")
    {
        error("Invalid D-Bus property type, TYPE={PROP_TYPE}", "PROP_TYPE",
              dBusMap->propertyType);
        throw std::invalid_argument("Unknown D-BUS property type");
    }

    for (const auto& dbusValue : dbusVals)
    {
        value = static_cast<std::string>(dbusValue);
        valMap.emplace(value, possibleBootSources[index]);
        index++;
    }
}

} // namespace pldm::responder::bios
