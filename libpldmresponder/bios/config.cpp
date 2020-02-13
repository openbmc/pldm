#include "config.hpp"

#include "enum_attribute.hpp"
#include "integer_attribute.hpp"
#include "string_attribute.hpp"

#include <fstream>
#include <iostream>

namespace pldm
{
namespace responder
{
namespace bios
{
namespace wip
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

BIOSConfig::BIOSConfig(const char* jsonDir, const char* tableDir) :
    jsonDir(jsonDir), tableDir(tableDir)
{
}

void BIOSConfig::buildTables()
{
    fs::create_directory(tableDir);
    auto stringTable = buildStringTable();
    if (stringTable)
    {
        buildAttrTables(*stringTable);
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
    return loadBIOSTable(tablePath);
}

void BIOSConfig::buildAttrTables(const Table& stringTable)
{
    BIOSStringTable biosStringTable(stringTable);
    load(jsonDir / stringJsonFile, [&biosStringTable, this](const Json& entry) {
        constructAttributes<BIOSStringAttribute>(entry);
    });
    load(jsonDir / integerJsonFile,
         [&biosStringTable, this](const Json& entry) {
             constructAttributes<BIOSIntegerAttribute>(entry);
         });

    if (biosAttributes.empty())
    {
        return;
    }

    Table attrTable, attrValueTable;
    attrTable.reserve(4096);
    attrValueTable.reserve(4096);

    for (auto& attr : biosAttributes)
    {
        try
        {
            attr->constructEntry(biosStringTable, attrTable, attrValueTable);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Construct Table Entry Error, AttributeName = "
                      << attr->name << std::endl;
        }
    }

    appendPadAndCheckSum(attrTable);
    appendPadAndCheckSum(attrValueTable);

    storeBIOSTable(tableDir / attrTableFile, attrTable);
    storeBIOSTable(tableDir / attrValueTableFile, attrValueTable);
}

std::optional<Table> BIOSConfig::buildStringTable()
{
    std::set<std::string> strings;
    auto handler = [&strings](const Json& entry) {
        try
        {
            strings.emplace(entry.at("attribute_name"));
        }
        catch (const std::exception& e)
        {
            std::cerr << "Get Attribute Name Error, " << e.what() << std::endl;
        }
    };

    load(jsonDir / stringJsonFile, handler);
    load(jsonDir / integerJsonFile, handler);
    load(jsonDir / enumJsonFile, [&strings](const Json& entry) {
        try
        {
            strings.emplace(entry.at("attribute_name"));
            auto possibleValues = entry.at("possible_values");
            for (auto& pv : possibleValues)
            {
                strings.emplace(pv);
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Get Strings From Enum Entry Error, " << e.what()
                      << std::endl;
        }
    });

    if (strings.empty())
    {
        return std::nullopt;
    }

    return std::make_optional<Table>(constructStringEntries(strings));
}

Table BIOSConfig::constructStringEntries(const std::set<std::string>& strings)
{
    Table table;
    table.reserve(4096);
    for (const auto& elem : strings)
    {
        auto tableSize = table.size();
        auto entryLength =
            pldm_bios_table_string_entry_encode_length(elem.length());
        table.resize(tableSize + entryLength);
        pldm_bios_table_string_entry_encode(
            table.data() + tableSize, entryLength, elem.c_str(), elem.length());
    }
    appendPadAndCheckSum(table);
    storeBIOSTable(tableDir / stringTableFile, table);
    return table;
}

void BIOSConfig::appendPadAndCheckSum(Table& table)
{
    auto sizeWithoutPad = table.size();
    auto padAndChecksumSize = pldm_bios_table_pad_checksum_size(sizeWithoutPad);
    table.resize(table.size() + padAndChecksumSize);

    pldm_bios_table_append_pad_checksum(table.data(), table.size(),
                                        sizeWithoutPad);
}

void BIOSConfig::storeBIOSTable(const fs::path& path, const Table& table)
{
    BIOSTable biosTable(path.c_str());
    biosTable.store(table);
}

std::optional<Table> BIOSConfig::loadBIOSTable(const fs::path& path)
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
        file.open(filePath);
        try
        {
            jsonConf = Json::parse(file);
            auto entries = jsonConf.at("entries");
            for (auto& entry : entries)
            {
                handler(entry);
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
    auto attrTable = getBIOSTable(PLDM_BIOS_ATTR_TABLE);
    auto stringTable = getBIOSTable(PLDM_BIOS_STRING_TABLE);
    if (!attrValueTable || !attrTable || !stringTable)
    {
        return PLDM_BIOS_TABLE_UNAVAILABLE;
    }

    size_t destBufferLength = attrValueTable->size() + size + 3;
    Table destTable(destBufferLength);
    size_t destTableLen = destTable.size();

    auto rc = pldm_bios_table_attr_value_copy_and_update(
        attrValueTable->data(), attrValueTable->size(), destTable.data(),
        &destTableLen, entry, size);
    destTable.resize(destTableLen);

    if (rc != PLDM_SUCCESS)
    {
        return rc;
    }
    auto attrValueEntry =
        reinterpret_cast<const pldm_bios_attr_val_table_entry*>(entry);
    auto attrHandle = pldm_bios_table_attr_value_entry_decode_attribute_handle(
        attrValueEntry);

    auto attrEntry = pldm_bios_table_attr_find_by_handle(
        attrTable->data(), attrTable->size(), attrHandle);
    assert(attrEntry != nullptr);

    auto attrNameHandle =
        pldm_bios_table_attr_entry_decode_string_handle(attrEntry);
    BIOSStringTable biosStringTable(*stringTable);
    auto attrName = biosStringTable.findString(attrNameHandle);

    auto iter = std::find_if(
        biosAttributes.begin(), biosAttributes.end(),
        [&attrName](const auto& attr) { return attr->name == attrName; });

    if (iter == biosAttributes.end())
    {
        return PLDM_ERROR;
    }

    rc =
        (*iter)->setAttrValueOnDbus(attrValueEntry, attrEntry, biosStringTable);
    if (rc != PLDM_SUCCESS)
    {
        return rc;
    }

    BIOSTable biosAttrValueTable((tableDir / attrValueTableFile).c_str());
    biosAttrValueTable.store(destTable);
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
    }
}

} // namespace wip
} // namespace bios
} // namespace responder
} // namespace pldm
