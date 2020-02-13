#include "bios_integer_attribute.hpp"

#include "utils.hpp"

namespace pldm
{
namespace responder
{
namespace bios
{
namespace wip
{

BIOSIntegerAttribute::BIOSIntegerAttribute(const Json& entry,
                                           const BIOSStringTable& stringTable) :
    BIOSAttribute(entry)
{
    std::string attr = entry.at("attribute_name");

    uint64_t lowerBound = entry.at("lower_bound");
    uint64_t upperBound = entry.at("upper_bound");
    uint32_t scalarIncrement = entry.at("scalar_increment");
    uint64_t defaultValue = entry.at("default_value");
    integerEntryInfo = {
        stringTable.findHandle(name),
        readOnly,
        lowerBound,
        upperBound,
        scalarIncrement,
        defaultValue,
    };
    const char* errmsg = nullptr;
    auto rc = pldm_bios_table_attr_entry_integer_info_check(&integerEntryInfo,
                                                            &errmsg);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Wrong filed for integer attribute, ATTRIBUTE_NAME="
                  << attr.c_str() << " ERRMSG=" << errmsg
                  << " LOWER_BOUND=" << lowerBound
                  << " UPPER_BOUND=" << upperBound
                  << " DEFAULT_VALUE=" << defaultValue
                  << " SCALAR_INCREMENT=" << scalarIncrement << "\n";
        throw std::invalid_argument("Wrong field for integer attribute");
    }
}

void BIOSIntegerAttribute::updateDbusProperty(uint64_t value)
{
    auto setDbusProperty = [this](const auto& variant) {
        pldm::utils::DBusHandler().setDbusProperty(
            dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
            dBusMap->interface.c_str(), variant);
    };

    if (dBusMap->propertyType == "uint8_t")
    {
        std::variant<uint8_t> v = value;
        setDbusProperty(v);
    }
    else if (dBusMap->propertyType == "int16_t")
    {
        std::variant<int16_t> v = value;
        setDbusProperty(v);
    }
    else if (dBusMap->propertyType == "uint16_t")
    {
        std::variant<uint16_t> v = value;
        setDbusProperty(v);
    }
    else if (dBusMap->propertyType == "int32_t")
    {
        std::variant<int32_t> v = value;
        setDbusProperty(v);
    }
    else if (dBusMap->propertyType == "uint32_t")
    {
        std::variant<uint32_t> v = value;
        setDbusProperty(v);
    }
    else if (dBusMap->propertyType == "int64_t")
    {
        std::variant<int64_t> v = value;
        setDbusProperty(v);
    }
    else if (dBusMap->propertyType == "uint64_t")
    {
        std::variant<uint64_t> v = value;
        setDbusProperty(v);
    }
    else
    {
        assert(false && "Unsupported Dbus Type");
    }
}

int BIOSIntegerAttribute::setAttrValueOnDbus(
    const pldm_bios_attr_val_table_entry* attrValueEntry,
    const pldm_bios_attr_table_entry*, const BIOSStringTable&)
{
    if (readOnly)
    {
        return PLDM_ERROR;
    }
    uint64_t currentValue =
        pldm_bios_table_attr_value_entry_integer_decode_cv(attrValueEntry);
        
    try
    {
        updateDbusProperty(currentValue);
        return PLDM_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr << "setAttributeValueOnDbus Error AttributeName = " << name
                  << std::endl;
        return PLDM_ERROR;
    }
}

void BIOSIntegerAttribute::constructEntry(const BIOSStringTable&,
                                          Table& attrTable,
                                          Table& attrValueTable)
{
    auto entryLength = pldm_bios_table_attr_entry_integer_encode_length();
    auto tableSize = attrTable.size();
    attrTable.resize(tableSize + entryLength, 0);
    pldm_bios_table_attr_entry_integer_encode(attrTable.data() + tableSize,
                                              entryLength, &integerEntryInfo);
    auto attrTableEntry = reinterpret_cast<pldm_bios_attr_table_entry*>(
        attrTable.data() + tableSize);

    auto attrHandle =
        pldm_bios_table_attr_entry_decode_attribute_handle(attrTableEntry);
    auto attrType =
        pldm_bios_table_attr_entry_decode_attribute_type(attrTableEntry);

    auto currentValue = getAttrValue();
    entryLength = pldm_bios_table_attr_value_entry_encode_integer_length();

    tableSize = attrValueTable.size();
    attrValueTable.resize(tableSize + entryLength);
    pldm_bios_table_attr_value_entry_encode_integer(
        attrValueTable.data() + tableSize, entryLength, attrHandle, attrType,
        currentValue);
}

uint64_t BIOSIntegerAttribute::getAttrValue()
{
    if (readOnly)
    {
        return integerEntryInfo.default_value;
    }

    try
    {
        return pldm::utils::DBusHandler().getDbusProperty<uint64_t>(
            dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
            dBusMap->interface.c_str());
    }
    catch (const std::exception& e)
    {
        std::cerr << "Get Integer Attribute Value Error: AttributeName = "
                  << name << std::endl;
        return integerEntryInfo.default_value;
    }
}

} // namespace wip
} // namespace bios
} // namespace responder
} // namespace pldm