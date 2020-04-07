#include "bios_config.hpp"

#include "bios_enum_attribute.hpp"
#include "bios_integer_attribute.hpp"
#include "bios_string_attribute.hpp"
#include "bios_table.hpp"

#include <fstream>
#include <iostream>

namespace pldm
{
namespace responder
{
namespace bios
{
namespace
{

constexpr auto enumJsonFile = "enum_attrs.json";
constexpr auto stringJsonFile = "string_attrs.json";
constexpr auto integerJsonFile = "integer_attrs.json";

constexpr auto stringTableFile = "stringTable";
constexpr auto attrTableFile = "attributeTable";
constexpr auto attrValueTableFile = "attributeValueTable";

} // namespace

BIOSConfig::BIOSConfig(const char* jsonDir, const char* tableDir,
                       DBusHandler* const dbusHandler) :
    jsonDir(jsonDir),
    tableDir(tableDir), dbusHandler(dbusHandler)
{
}

void BIOSConfig::buildTables()
{
    fs::create_directory(tableDir);
    auto stringTable = buildAndStoreStringTable();
    if (stringTable)
    {
        constructAttributes(*stringTable);
        buildAndStoreAttrTables(*stringTable);
    }
}

std::optional<Table> BIOSConfig::getBIOSTable(pldm_bios_table_types tableType)
{
    fs::path tablePath;
    switch (tableType)
    {
        case PLDM_BIOS_STRING_TABLE:
            tablePath = tableDir / stringTableFile;
            break;
        case PLDM_BIOS_ATTR_TABLE:
            tablePath = tableDir / attrTableFile;
            break;
        case PLDM_BIOS_ATTR_VAL_TABLE:
            tablePath = tableDir / attrValueTableFile;
            break;
    }
    return loadTable(tablePath);
}

void BIOSConfig::constructAttributes(const BIOSStringTable& stringTable)
{
    load(jsonDir / stringJsonFile, [this, &stringTable](const Json& entry) {
        constructAttribute<BIOSStringAttribute>(entry, stringTable);
    });
    load(jsonDir / integerJsonFile, [this, &stringTable](const Json& entry) {
        constructAttribute<BIOSIntegerAttribute>(entry, stringTable);
    });
    load(jsonDir / enumJsonFile, [this, &stringTable](const Json& entry) {
        constructAttribute<BIOSEnumAttribute>(entry, stringTable);
    });
}

void BIOSConfig::buildAndStoreAttrTables(const Table& stringTable)
{
    BIOSStringTable biosStringTable(stringTable);

    if (biosAttributes.empty())
    {
        return;
    }

    Table attrTable, attrValueTable;

    for (auto& attr : biosAttributes)
    {
        try
        {
            attr->constructEntry(attrTable, attrValueTable);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Construct Table Entry Error, AttributeName = "
                      << attr->name << std::endl;
        }
    }

    table::appendPadAndChecksum(attrTable);
    table::appendPadAndChecksum(attrValueTable);

    storeTable(tableDir / attrTableFile, attrTable);
    storeTable(tableDir / attrValueTableFile, attrValueTable);
}

std::optional<Table> BIOSConfig::buildAndStoreStringTable()
{
    std::set<std::string> strings;
    auto handler = [&strings](const Json& entry) {
        strings.emplace(entry.at("attribute_name"));
    };

    load(jsonDir / stringJsonFile, handler);
    load(jsonDir / integerJsonFile, handler);
    load(jsonDir / enumJsonFile, [&strings](const Json& entry) {
        strings.emplace(entry.at("attribute_name"));
        auto possibleValues = entry.at("possible_values");
        for (auto& pv : possibleValues)
        {
            strings.emplace(pv);
        }
    });

    if (strings.empty())
    {
        return std::nullopt;
    }

    Table table;
    for (const auto& elem : strings)
    {
        table::string::constructEntry(table, elem);
    }

    table::appendPadAndChecksum(table);
    storeTable(tableDir / stringTableFile, table);
    return table;
}

void BIOSConfig::storeTable(const fs::path& path, const Table& table)
{
    BIOSTable biosTable(path.c_str());
    biosTable.store(table);
}

std::optional<Table> BIOSConfig::loadTable(const fs::path& path)
{
    BIOSTable biosTable(path.c_str());
    if (biosTable.isEmpty())
    {
        return std::nullopt;
    }

    Table table;
    biosTable.load(table);
    return table;
}

void BIOSConfig::load(const fs::path& filePath, ParseHandler handler)
{
    std::ifstream file;
    Json jsonConf;
    if (fs::exists(filePath))
    {
        try
        {
            file.open(filePath);
            jsonConf = Json::parse(file);
            auto entries = jsonConf.at("entries");
            for (auto& entry : entries)
            {
                try
                {
                    handler(entry);
                }
                catch (const std::exception& e)
                {
                    std::cerr
                        << "Failed to parse JSON config file(entry handler) : "
                        << filePath.c_str() << ", " << e.what() << std::endl;
                }
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to parse JSON config file : "
                      << filePath.c_str() << std::endl;
        }
    }
}

int BIOSConfig::setAttrValue(const void* entry, size_t size)
{
    auto attrValueTable = getBIOSTable(PLDM_BIOS_ATTR_VAL_TABLE);

    if (!attrValueTable)
    {
        return PLDM_BIOS_TABLE_UNAVAILABLE;
    }

    auto destTable =
        table::attribute_value::updateTable(*attrValueTable, entry, size);

    if (!destTable)
    {
        return PLDM_ERROR;
    }
    auto attrValueEntry =
        reinterpret_cast<const pldm_bios_attr_val_table_entry*>(entry);

    auto [attrHandle, _] = table::attribute_value::decodeHeader(attrValueEntry);

    try
    {
        auto iter = std::find_if(biosAttributes.begin(), biosAttributes.end(),
                                 [&attrHandle](const auto& attr) {
                                     return attr->attrHandle == attrHandle;
                                 });

        if (iter == biosAttributes.end())
        {
            return PLDM_ERROR;
        }

        (*iter)->setAttrValueOnDbus(attrValueEntry);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Set attribute value error: " << e.what() << std::endl;
        return PLDM_ERROR;
    }

    BIOSTable biosAttrValueTable((tableDir / attrValueTableFile).c_str());
    biosAttrValueTable.store(*destTable);
    return PLDM_SUCCESS;
}

void BIOSConfig::removeTables()
{
    try
    {
        fs::remove(tableDir / stringTableFile);
        fs::remove(tableDir / attrTableFile);
        fs::remove(tableDir / attrValueTableFile);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Remove the tables error: " << e.what() << std::endl;
    }
}

void BIOSConfig::processBiosAttrChangeNotification(
    const DbusChObjProperties& chProperties, uint32_t biosAttrIndex)
{
    const auto& dBusMap = biosAttributes[biosAttrIndex]->getDBusMap();
    const auto& propertyName = dBusMap->propertyName;
    const auto& attrName = biosAttributes[biosAttrIndex]->name;

    const auto it = chProperties.find(propertyName);
    if (it == chProperties.end())
    {
        return;
    }

    PropertyValue newPropVal = it->second;

    auto attrValueSrcTable = getBIOSTable(PLDM_BIOS_ATTR_VAL_TABLE);

    if (!attrValueSrcTable.has_value())
    {
        std::cerr << "Attribute value table not present\n";
        return;
    }

    Table newValue;
    auto rc =
        biosAttributes[biosAttrIndex]->updateAttrVal(newValue, newPropVal);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Could not update the attribute value table for attribute "
                     "name="
                  << attrName << "\n";

        return;
    }
    auto destTable = table::attribute_value::updateTable(
        *attrValueSrcTable, newValue.data(), newValue.size());
    if (destTable.has_value())
    {
        storeTable(tableDir / attrValueTableFile, *destTable);
    }
}

} // namespace bios
} // namespace responder
} // namespace pldm
