#include "bios_config.hpp"

#include "bios_enum_attribute.hpp"
#include "bios_integer_attribute.hpp"
#include "bios_string_attribute.hpp"

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

template <typename T>
void constructAttributes(BIOSAttributes& biosAttributes, const Json& entry,
                         const BIOSStringTable& biosStringTable)
{
    try
    {
        biosAttributes.push_back(std::make_unique<T>(entry, biosStringTable));
    }
    catch (const std::exception& e)
    {
        std::cerr << "Constructs Attribute Error, " << e.what() << std::endl;
    }
}

} // namespace

BIOSConfig::BIOSConfig(const fs::path& jsonDir, const fs::path& tableDir) :
    jsonDir(jsonDir), tableDir(tableDir)
{
    auto stringTable = buildStringTable();
    if (stringTable)
    {
        buildAttrTables(*stringTable);
    }
}

std::optional<Table> BIOSConfig::getBIOSStringTable()
{
    return loadBIOSTable(tableDir / stringTableFile);
}

std::optional<Table> BIOSConfig::getBIOSAttrTable()
{
    return loadBIOSTable(tableDir / attrTableFile);
}

std::optional<Table> BIOSConfig::getBIOSAttrValueTable()
{
    return loadBIOSTable(tableDir / attrValueTableFile);
}

void BIOSConfig::buildAttrTables(const Table& stringTable)
{
    BIOSStringTable biosStringTable(stringTable);
    load(jsonDir / stringJsonFile, [&biosStringTable, this](const Json& entry) {
        constructAttributes<BIOSStringAttribute>(biosAttributes, entry,
                                                 biosStringTable);
    });
    load(jsonDir / integerJsonFile,
         [&biosStringTable, this](const Json& entry) {
             constructAttributes<BIOSIntegerAttribute>(biosAttributes, entry,
                                                       biosStringTable);
         });
    load(jsonDir / enumJsonFile, [&biosStringTable, this](const Json& entry) {
        constructAttributes<BIOSEnumAttribute>(biosAttributes, entry,
                                               biosStringTable);
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
        attr->constructEntry(biosStringTable, attrTable, attrValueTable);
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
    auto attrValueTable = getBIOSAttrValueTable();
    auto attrTable = getBIOSAttrValueTable();
    auto stringTable = getBIOSStringTable();
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

} // namespace wip
} // namespace bios
} // namespace responder
} // namespace pldm