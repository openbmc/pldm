#include "bios_config.hpp"

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
    buildAttrTables(stringTable);
}

Table BIOSConfig::getBIOSStringTable()
{
    return loadBIOSTable(tableDir / stringTableFile);
}

Table BIOSConfig::getBIOSAttrTable()
{
    return loadBIOSTable(tableDir / attrTableFile);
}

Table BIOSConfig::getBIOSAttrValueTable()
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

Table BIOSConfig::buildStringTable()
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

    return constructStringEntries(strings);
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

Table BIOSConfig::loadBIOSTable(const fs::path& path)
{
    Table table;
    BIOSTable biosTable(path.c_str());
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

void BIOSConfig::setAttrValue(const void* entry, size_t size)
{
    BIOSTable biosAttrValueTable((tableDir / attrValueTableFile).c_str());
    Table srcTable;
    biosAttrValueTable.load(srcTable);

    size_t destBufferLength = srcTable.size() + size + 3;
    Table destTable(destBufferLength);
    size_t destTableLen = destTable.size();

    auto rc = pldm_bios_table_attr_value_copy_and_update(
        srcTable.data(), srcTable.size(), destTable.data(), &destTableLen,
        entry, size);
    destTable.resize(destTableLen);

    if (rc != PLDM_SUCCESS)
    {
        // TODO
    }
    auto attrValueEntry =
        reinterpret_cast<const pldm_bios_attr_val_table_entry*>(entry);
    auto attrHandle = pldm_bios_table_attr_value_entry_decode_attribute_handle(
        attrValueEntry);

    auto attributeTable = getBIOSAttrTable();
    auto attrEntry = pldm_bios_table_attr_find_by_handle(
        attributeTable.data(), attributeTable.size(), attrHandle);
    assert(attrEntry != nullptr);

    auto attrNameHandle =
        pldm_bios_table_attr_entry_decode_string_handle(attrEntry);
    BIOSStringTable stringTable(getBIOSStringTable());
    auto attrName = stringTable.findString(attrNameHandle);

    auto iter = std::find_if(
        biosAttributes.begin(), biosAttributes.end(),
        [&attrName](const auto& attr) { return attr->name == attrName; });

    if (iter != biosAttributes.end())
    {

        (*iter)->setAttrValueOnDbus(attrValueEntry, attrEntry, stringTable);
    }
    biosAttrValueTable.store(destTable);
}

} // namespace wip
} // namespace bios
} // namespace responder
} // namespace pldm