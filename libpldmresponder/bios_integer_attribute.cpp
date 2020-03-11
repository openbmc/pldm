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

    integerInfo.lowerBound = entry.at("lower_bound");
    integerInfo.upperBound = entry.at("upper_bound");
    integerInfo.scalarIncrement = entry.at("scalar_increment");
    integerInfo.defaultValue = entry.at("default_value");
    pldm_bios_table_attr_entry_integer_info info = {
        0,
        readOnly,
        integerInfo.lowerBound,
        integerInfo.upperBound,
        integerInfo.scalarIncrement,
        integerInfo.defaultValue,
    };
    const char* errmsg = nullptr;
    auto rc = pldm_bios_table_attr_entry_integer_info_check(&info, &errmsg);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Wrong filed for integer attribute, ATTRIBUTE_NAME="
                  << attr.c_str() << " ERRMSG=" << errmsg
                  << " LOWER_BOUND=" << integerInfo.lowerBound
                  << " UPPER_BOUND=" << integerInfo.upperBound
                  << " DEFAULT_VALUE=" << integerInfo.defaultValue
                  << " SCALAR_INCREMENT=" << integerInfo.scalarIncrement
                  << "\n";
        throw std::invalid_argument("Wrong field for integer attribute");
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
    auto currentValue =
        table::attribute_value::decodeIntegerEntry(attrValueEntry);

    if (dBusMap->propertyType == "uint8_t")
    {
        return dbusHandler->setDbusProperty(*dBusMap,
                                            static_cast<uint8_t>(currentValue));
    }
    else if (dBusMap->propertyType == "uint16_t")
    {
        return dbusHandler->setDbusProperty(
            *dBusMap, static_cast<uint16_t>(currentValue));
    }
    else if (dBusMap->propertyType == "int16_t")
    {
        return dbusHandler->setDbusProperty(*dBusMap,
                                            static_cast<int16_t>(currentValue));
    }
    else if (dBusMap->propertyType == "uint32_t")
    {
        return dbusHandler->setDbusProperty(
            *dBusMap, static_cast<uint32_t>(currentValue));
    }
    else if (dBusMap->propertyType == "int32_t")
    {
        return dbusHandler->setDbusProperty(*dBusMap,
                                            static_cast<int32_t>(currentValue));
    }
    else if (dBusMap->propertyType == "uint64_t")
    {
        return dbusHandler->setDbusProperty(*dBusMap, currentValue);
    }
    else if (dBusMap->propertyType == "int64_t")
    {
        return dbusHandler->setDbusProperty(*dBusMap,
                                            static_cast<int64_t>(currentValue));
    }

    std::cerr << "Unsupported property type on dbus: " << dBusMap->propertyType
              << std::endl;
    throw std::invalid_argument("dbus type error");
}

void BIOSIntegerAttribute::constructEntry(const BIOSStringTable& stringTable,
                                          Table& attrTable,
                                          Table& attrValueTable)
{

    pldm_bios_table_attr_entry_integer_info info = {
        stringTable.findHandle(name), readOnly,
        integerInfo.lowerBound,       integerInfo.upperBound,
        integerInfo.scalarIncrement,  integerInfo.defaultValue,
    };

    auto attrTableEntry =
        table::attribute::constructIntegerEntry(attrTable, &info);

    auto [attrHandle, attrType, _] =
        table::attribute::decodeHeader(attrTableEntry);

    auto currentValue = getAttrValue();
    table::attribute_value::constructIntegerEntry(attrValueTable, attrHandle,
                                                  attrType, currentValue);
}

uint64_t BIOSIntegerAttribute::getAttrValue()
{
    if (readOnly)
    {
        return integerInfo.defaultValue;
    }

    try
    {
        uint64_t value;
        if (dBusMap->propertyType == "uint8_t")
        {
            value = dbusHandler->getDbusProperty<uint8_t>(
                dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
                dBusMap->interface.c_str());
        }
        else if (dBusMap->propertyType == "uint16_t")
        {
            value = dbusHandler->getDbusProperty<uint16_t>(
                dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
                dBusMap->interface.c_str());
        }
        else if (dBusMap->propertyType == "int16_t")
        {
            value = dbusHandler->getDbusProperty<int16_t>(
                dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
                dBusMap->interface.c_str());
        }
        else if (dBusMap->propertyType == "uint32_t")
        {
            value = dbusHandler->getDbusProperty<uint32_t>(
                dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
                dBusMap->interface.c_str());
        }
        else if (dBusMap->propertyType == "int32_t")
        {
            value = dbusHandler->getDbusProperty<int32_t>(
                dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
                dBusMap->interface.c_str());
        }
        else if (dBusMap->propertyType == "uint64_t")
        {
            value = dbusHandler->getDbusProperty<uint64_t>(
                dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
                dBusMap->interface.c_str());
        }
        else if (dBusMap->propertyType == "int64_t")
        {
            value = dbusHandler->getDbusProperty<int64_t>(
                dBusMap->objectPath.c_str(), dBusMap->propertyName.c_str(),
                dBusMap->interface.c_str());
        }
        return value;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Get Integer Attribute Value Error: AttributeName = "
                  << name << std::endl;
        return integerInfo.defaultValue;
    }
}

} // namespace bios
} // namespace responder
} // namespace pldm