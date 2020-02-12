#include "bios_string_attribute.hpp"

#include "utils.hpp"

#include <iostream>
#include <variant>

namespace pldm
{
namespace responder
{
namespace bios
{
namespace wip
{

BIOSStringAttribute::BIOSStringAttribute(const Json& entry,
                                         const BIOSStringTable& stringTable) :
    BIOSAttribute(entry)
{
    std::string strTypeTmp = entry.at("string_type");
    auto iter = strTypeMap.find(strTypeTmp);
    if (iter == strTypeMap.end())
    {
        std::cerr << "Wrong string type, STRING_TYPE=" << strTypeTmp
                  << " ATTRIBUTE_NAME=" << name << "\n";
        throw std::invalid_argument("Wrong string type");
    }
    uint8_t strType = iter->second;

    uint16_t minStrLen = entry.at("minimum_string_length");
    uint16_t maxStrLen = entry.at("maximum_string_length");
    uint16_t defaultStrLen = entry.at("default_string_length");
    std::string defaultStr = entry.at("default_string");

    stringEntryInfo = {
        stringTable.findHandle(name),
        readOnly,
        strType,
        minStrLen,
        maxStrLen,
        defaultStrLen,
        defaultStr.data(),
    };

    const char* errmsg;
    auto rc =
        pldm_bios_table_attr_entry_string_info_check(&stringEntryInfo, &errmsg);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Wrong field for string attribute, ATTRIBUTE_NAME=" << name
                  << " ERRMSG=" << errmsg
                  << " MINIMUM_STRING_LENGTH=" << minStrLen
                  << " MAXIMUM_STRING_LENGTH=" << maxStrLen
                  << " DEFAULT_STRING_LENGTH=" << defaultStrLen
                  << " DEFAULT_STRING=" << defaultStr.data() << "\n";
        throw std::invalid_argument("Wrong field for string attribute");
    }
}

std::string BIOSStringAttribute::stringToUtf8(BIOSStringEncoding stringType,
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

void BIOSStringAttribute::setAttrValueOnDbus(
    const pldm_bios_attr_val_table_entry* attrValueEntry,
    const pldm_bios_attr_table_entry*, const BIOSStringTable&)
{
    if (readOnly)
        return;

    variable_field currentString{};
    pldm_bios_table_attr_value_entry_string_decode_string(attrValueEntry,
                                                          &currentString);
    std::vector<uint8_t> data(currentString.ptr,
                              currentString.ptr + currentString.length);

    std::variant<std::string> value = stringToUtf8(
        static_cast<BIOSStringEncoding>(stringEntryInfo.string_type), data);

    pldm::utils::DBusHandler().setDbusProperty(
        dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
        dBusMap->interface.c_str(), value);
}

std::string BIOSStringAttribute::getAttrValue()
{
    if (readOnly)
    {
        return std::string(stringEntryInfo.def_string,
                           stringEntryInfo.def_length);
    }

    return pldm::utils::DBusHandler().getDbusProperty<std::string>(
        dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
        dBusMap->interface.c_str());
}

void BIOSStringAttribute::constructEntry(const BIOSStringTable&,
                                         Table& attrTable,
                                         Table& attrValueTable)
{
    auto entryLength = pldm_bios_table_attr_entry_string_encode_length(
        stringEntryInfo.def_length);

    auto tableSize = attrTable.size();
    attrTable.resize(tableSize + entryLength, 0);
    pldm_bios_table_attr_entry_string_encode(attrTable.data() + tableSize,
                                             entryLength, &stringEntryInfo);

    auto attrTableEntry = reinterpret_cast<pldm_bios_attr_table_entry*>(
        attrTable.data() + tableSize);

    auto attrHandle =
        pldm_bios_table_attr_entry_decode_attribute_handle(attrTableEntry);
    auto attrType =
        pldm_bios_table_attr_entry_decode_attribute_type(attrTableEntry);

    auto currStr = getAttrValue();
    auto currStrLen = currStr.size();
    entryLength =
        pldm_bios_table_attr_value_entry_encode_string_length(currStrLen);
    tableSize = attrValueTable.size();
    attrValueTable.resize(tableSize + entryLength);
    pldm_bios_table_attr_value_entry_encode_string(
        attrValueTable.data() + tableSize, entryLength, attrHandle, attrType,
        currStrLen, currStr.c_str());
}

} // namespace wip
} // namespace bios
} // namespace responder
} // namespace pldm
