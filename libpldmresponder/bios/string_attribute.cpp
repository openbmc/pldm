#include "string_attribute.hpp"

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

BIOSStringAttribute::BIOSStringAttribute(const Json& entry) :
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
    stringType = iter->second;

    minStringLength = entry.at("minimum_string_length");
    maxStringLength = entry.at("maximum_string_length");
    defaultStringLength = entry.at("default_string_length");
    defaultString = entry.at("default_string");

    pldm_bios_table_attr_entry_string_info info = {
        0,
        readOnly,
        stringType,
        minStringLength,
        maxStringLength,
        defaultStringLength,
        defaultString.data(),
    };

    const char* errmsg;
    auto rc = pldm_bios_table_attr_entry_string_info_check(&info, &errmsg);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Wrong field for string attribute, ATTRIBUTE_NAME=" << name
                  << " ERRMSG=" << errmsg
                  << " MINIMUM_STRING_LENGTH=" << minStringLength
                  << " MAXIMUM_STRING_LENGTH=" << maxStringLength
                  << " DEFAULT_STRING_LENGTH=" << defaultStringLength
                  << " DEFAULT_STRING=" << defaultString.data() << "\n";
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

int BIOSStringAttribute::setAttrValueOnDbus(
    const pldm_bios_attr_val_table_entry* attrValueEntry,
    const pldm_bios_attr_table_entry* attrEntry, const BIOSStringTable&)
{
    if (readOnly)
        return PLDM_ERROR;

    variable_field currentString{};
    pldm_bios_table_attr_value_entry_string_decode_string(attrValueEntry,
                                                          &currentString);
    auto strType =
        pldm_bios_table_attr_entry_string_decode_string_type(attrEntry);
    std::vector<uint8_t> data(currentString.ptr,
                              currentString.ptr + currentString.length);

    std::variant<std::string> value =
        stringToUtf8(static_cast<BIOSStringEncoding>(strType), data);

    try
    {
        pldm::utils::DBusHandler().setDbusProperty(
            dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
            dBusMap->interface.c_str(), value);
        return PLDM_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr << "setAttributeValueOnDbus Error AttributeName = " << name
                  << std::endl;
        return PLDM_ERROR;
    }
}

std::string BIOSStringAttribute::getAttrValue()
{
    if (readOnly)
    {
        return defaultString;
    }
    try
    {
        return pldm::utils::DBusHandler().getDbusProperty<std::string>(
            dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
            dBusMap->interface.c_str());
    }
    catch (const std::exception& e)
    {
        std::cerr << "Get String Attribute Value Error: AttributeName = "
                  << name << std::endl;
        return defaultString;
    }
}

void BIOSStringAttribute::constructEntry(const BIOSStringTable& stringTable,
                                         Table& attrTable,
                                         Table& attrValueTable)
{
    pldm_bios_table_attr_entry_string_info info = {
        stringTable.findHandle(name),
        readOnly,
        stringType,
        minStringLength,
        maxStringLength,
        defaultStringLength,
        defaultString.data(),
    };
    auto entryLength =
        pldm_bios_table_attr_entry_string_encode_length(info.def_length);

    auto tableSize = attrTable.size();
    attrTable.resize(tableSize + entryLength, 0);
    pldm_bios_table_attr_entry_string_encode(attrTable.data() + tableSize,
                                             entryLength, &info);

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
