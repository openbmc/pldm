#include "bios_integer_attribute.hpp"

#include "utils.hpp"

namespace pldm
{
namespace responder
{
namespace bios
{

BIOSIntegerAttribute::BIOSIntegerAttribute(const Json& entry,
                                           DBusHandler* const dbusHandler) :
    BIOSAttribute(entry, dbusHandler)
{
    std::string attr = entry.at("attribute_name");

    lowerBound = entry.at("lower_bound");
    upperBound = entry.at("upper_bound");
    scalarIncrement = entry.at("scalar_increment");
    defaultValue = entry.at("default_value");
    pldm_bios_table_attr_entry_integer_info info = {
        0, readOnly, lowerBound, upperBound, scalarIncrement, defaultValue,
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
        throw std::invalid_argument("Wrong field for integer attribute");
    }
}

void BIOSIntegerAttribute::updateDbusProperty(uint64_t value)
{
    auto setDbusProperty = [this](const auto& variant) {
        dbusHandler->setDbusProperty(dBusMap->objectPath.c_str(),
                                     dBusMap->propertyName.c_str(),
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

void BIOSIntegerAttribute::setAttrValueOnDbus(
    const pldm_bios_attr_val_table_entry* attrValueEntry,
    const pldm_bios_attr_table_entry*, const BIOSStringTable&)
{
    if (readOnly)
    {
        return;
    }
    auto currentValue = BIOSAttrValTable::decodeIntegerEntry(attrValueEntry);

    updateDbusProperty(currentValue);
}

void BIOSIntegerAttribute::constructEntry(const BIOSStringTable& stringTable,
                                          Table& attrTable,
                                          Table& attrValueTable)
{

    pldm_bios_table_attr_entry_integer_info info = {
        stringTable.findHandle(name),
        readOnly,
        lowerBound,
        upperBound,
        scalarIncrement,
        defaultValue,
    };

    auto attrTableEntry =
        BIOSAttrTable::constructIntegerEntry(attrTable, &info);

    auto [attrHandle, attrType, _] =
        BIOSAttrTable::decodeHeader(attrTableEntry);

    auto currentValue = getAttrValue();
    BIOSAttrValTable::constructIntegerEntry(attrValueTable, attrHandle,
                                            attrType, currentValue);
}

uint64_t BIOSIntegerAttribute::getAttrValue()
{
    if (readOnly)
    {
        return defaultValue;
    }

    try
    {
        return dbusHandler->getDbusProperty<uint64_t>(
            dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
            dBusMap->interface.c_str());
    }
    catch (const std::exception& e)
    {
        std::cerr << "Get Integer Attribute Value Error: AttributeName = "
                  << name << std::endl;
        return defaultValue;
    }
}

} // namespace bios
} // namespace responder
} // namespace pldm