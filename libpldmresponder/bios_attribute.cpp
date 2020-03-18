#include "config.h"

#include "bios_attribute.hpp"

#include "utils.hpp"

#include <iostream>
#include <variant>
#include <xyz/openbmc_project/Common/error.hpp>

namespace pldm
{
namespace responder
{
namespace bios
{

using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

BIOSAttribute::BIOSAttribute(const Json& entry,
                             DBusHandler* const dbusHandler) :
    name(entry.at("attribute_name")),
    readOnly(!entry.contains("dbus")), dbusHandler(dbusHandler)
{
    chBiosAttrPropSignal = nullptr;
    if (!readOnly)
    {
        std::string objectPath = entry.at("dbus").at("object_path");
        std::string interface = entry.at("dbus").at("interface");
        std::string propertyName = entry.at("dbus").at("property_name");
        std::string propertyType = entry.at("dbus").at("property_type");

        dBusMap = {objectPath, interface, propertyName, propertyType};

        using namespace sdbusplus::bus::match::rules;
        chBiosAttrPropSignal = std::make_unique<sdbusplus::bus::match::match>(
            pldm::utils::DBusHandler::getBus(),
            propertiesChanged(dBusMap->objectPath, dBusMap->interface),
            [&](sdbusplus::message::message& msg) {
                DbusChObjProperties props;
                std::string iface;
                msg.read(iface, props);
                processBiosAttrChangeNotification(props, dBusMap->propertyName,
                                                  name);
            });
    }
}

void BIOSAttribute::processBiosAttrChangeNotification(
    const DbusChObjProperties& chProperties, const std::string& propertyName,
    const std::string& attrName)
{
    std::cout << "entered processBiosAttrChangeNotification for property "
              << propertyName.c_str() << " and attribute " << attrName.c_str()
              << "\n";
    const auto it = chProperties.find(propertyName);

    if (it == chProperties.end())
    {
        std::cout << "property " << propertyName.c_str()
                  << " has not changed\n";
        return;
    }
    std::pair<std::string, DbusVariant> matchedProp =
        std::make_pair(it->first, it->second);

    fs::path tablesPath(BIOS_TABLES_DIR);
    constexpr auto stringTableFile = "stringTable";
    auto stringTablePath = tablesPath / stringTableFile;
    BIOSStringTable biosStringTable(stringTablePath.c_str());
    uint16_t attrNameHdl;
    try
    {
        attrNameHdl = biosStringTable.findHandle(attrName);
        std::cout << "found string handle " << attrNameHdl << " for attribute "
                  << attrName.c_str() << "\n";
    }
    catch (InternalFailure& e)
    {
        std::cerr << "Could not find handle for BIOS string, ATTRIBUTE="
                  << attrName.c_str() << "\n";
        return;
    }

    Table attrTable;
    constexpr auto attrTableFile = "attributeTable";
    auto attrTablePath = tablesPath / attrTableFile;
    BIOSTable biosAttributeTable(attrTablePath.c_str());
    biosAttributeTable.load(attrTable);

    auto iter = pldm_bios_table_iter_create(attrTable.data(), attrTable.size(),
                                            PLDM_BIOS_ATTR_TABLE);
    static constexpr auto PLDM_BIOS_INVALID_TYPE = 0x9;
    uint8_t attrType = PLDM_BIOS_INVALID_TYPE;
    uint16_t attrHdl;
    const struct pldm_bios_attr_table_entry* tableEntry = nullptr;
    while (!pldm_bios_table_iter_is_end(iter))
    {
        tableEntry = pldm_bios_table_iter_attr_entry_value(iter);
        if (tableEntry->string_handle == attrNameHdl)
        {
            attrHdl = tableEntry->attr_handle;
            attrType = tableEntry->attr_type;
            break;
        }
        pldm_bios_table_iter_next(iter);
    }
    if (pldm_bios_table_iter_is_end(iter))
    {
        std::cerr << "Attribute not found in attribute table, name handle="
                  << attrNameHdl << "\n";
        pldm_bios_table_iter_free(iter);
        return;
    }
    pldm_bios_table_iter_free(iter);

    std::cout << "attribute handle is " << attrHdl << "\n";
    std::cout << "attribute type from attr table is " << (uint32_t)attrType
              << "\n";

    constexpr auto attrValTableFile = "attributeValueTable";
    auto attrValueTablePath = tablesPath / attrValTableFile;
    BIOSTable biosAttributeValueTable(attrValueTablePath.c_str());
    Table attrValueSrcTable;
    biosAttributeValueTable.load(attrValueSrcTable);

    Table newValue;
    std::copy_n(reinterpret_cast<uint8_t*>(&attrHdl), sizeof(attrHdl),
                std::back_inserter(newValue));
    std::copy_n(reinterpret_cast<uint8_t*>(&attrType), sizeof(attrType),
                std::back_inserter(newValue));

    auto rc = updateAttrVal(newValue, matchedProp, tableEntry);

    if (rc != PLDM_SUCCESS)
    {
        return;
    }

    auto destTable = table::attribute_value::updateTable(
        attrValueSrcTable, newValue.data(), newValue.size());
    if (destTable)
    {
        biosAttributeValueTable.store(*destTable);
    }
    std::cout << "exiting processBiosAttrChangeNotification " << std::endl;
}

} // namespace bios
} // namespace responder
} // namespace pldm
