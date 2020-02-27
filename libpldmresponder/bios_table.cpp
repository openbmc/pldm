#include "bios_table.hpp"

#include <fstream>

#include "bios_table.h"

namespace pldm
{

namespace responder
{

namespace bios
{

BIOSTable::BIOSTable(const char* filePath) : filePath(filePath)
{
}

bool BIOSTable::isEmpty() const noexcept
{
    bool empty = false;
    try
    {
        empty = fs::is_empty(filePath);
    }
    catch (fs::filesystem_error& e)
    {
        return true;
    }
    return empty;
}

void BIOSTable::store(const Table& table)
{
    std::ofstream stream(filePath.string(), std::ios::out | std::ios::binary);
    stream.write(reinterpret_cast<const char*>(table.data()), table.size());
}

void BIOSTable::load(Response& response) const
{
    auto currSize = response.size();
    auto fileSize = fs::file_size(filePath);
    response.resize(currSize + fileSize);
    std::ifstream stream(filePath.string(), std::ios::in | std::ios::binary);
    stream.read(reinterpret_cast<char*>(response.data() + currSize), fileSize);
}

BIOSStringTable::BIOSStringTable(const Table& stringTable) :
    stringTable(stringTable)
{
}

BIOSStringTable::BIOSStringTable(const BIOSTable& biosTable)
{
    biosTable.load(stringTable);
}

std::string BIOSStringTable::findString(uint16_t handle) const
{
    auto stringEntry = pldm_bios_table_string_find_by_handle(
        stringTable.data(), stringTable.size(), handle);
    if (stringEntry == nullptr)
    {
        throw std::invalid_argument("Invalid String Handle");
    }
    return decodeString(stringEntry);
}

uint16_t BIOSStringTable::findHandle(const std::string& name) const
{
    auto stringEntry = pldm_bios_table_string_find_by_string(
        stringTable.data(), stringTable.size(), name.c_str());
    if (stringEntry == nullptr)
    {
        throw std::invalid_argument("Invalid String Name");
    }

    return decodeHandle(stringEntry);
}

uint16_t BIOSStringTable::decodeHandle(
    const pldm_bios_string_table_entry* entry) const
{
    return pldm_bios_table_string_entry_decode_handle(entry);
}

std::string BIOSStringTable::decodeString(
    const pldm_bios_string_table_entry* entry) const
{
    auto strLength = pldm_bios_table_string_entry_decode_string_length(entry);
    std::vector<char> buffer(strLength + 1 /* sizeof '\0' */);
    pldm_bios_table_string_entry_decode_string(entry, buffer.data(),
                                               buffer.size());
    return std::string(buffer.data(), buffer.data() + strLength);
}

const pldm_bios_attr_table_entry* BIOSAttrTable::constructStringEntry(
    Table& table, pldm_bios_table_attr_entry_string_info* info)
{
    auto entryLength =
        pldm_bios_table_attr_entry_string_encode_length(info->def_length);

    auto tableSize = table.size();
    table.resize(tableSize + entryLength, 0);
    pldm_bios_table_attr_entry_string_encode(table.data() + tableSize,
                                             entryLength, info);
    return reinterpret_cast<pldm_bios_attr_table_entry*>(table.data() +
                                                         tableSize);
}

BIOSAttrTable::TableHeader
    BIOSAttrTable::decodeHeader(const pldm_bios_attr_table_entry* entry)
{
    auto attrHandle = pldm_bios_table_attr_entry_decode_attribute_handle(entry);
    auto attrType = pldm_bios_table_attr_entry_decode_attribute_type(entry);
    auto stringHandle = pldm_bios_table_attr_entry_decode_string_handle(entry);
    return {attrHandle, attrType, stringHandle};
}

BIOSAttrTable::StringField
    BIOSAttrTable::decodeStringEntry(const pldm_bios_attr_table_entry* entry)
{
    auto strType = pldm_bios_table_attr_entry_string_decode_string_type(entry);
    auto minLength = pldm_bios_table_attr_entry_string_decode_min_length(entry);
    auto maxLength = pldm_bios_table_attr_entry_string_decode_max_length(entry);
    auto defLength =
        pldm_bios_table_attr_entry_string_decode_def_string_length(entry);

    std::vector<char> buffer(defLength + 1);
    pldm_bios_table_attr_entry_string_decode_def_string(entry, buffer.data(),
                                                        buffer.size());
    return {strType, minLength, maxLength, defLength,
            std::string(buffer.data(), buffer.data() + defLength)};
}

BIOSAttrValTable::TableHeader
    BIOSAttrValTable::decodeHeader(const pldm_bios_attr_val_table_entry* entry)
{
    auto handle =
        pldm_bios_table_attr_value_entry_decode_attribute_handle(entry);
    auto type = pldm_bios_table_attr_value_entry_decode_attribute_type(entry);
    return {handle, type};
}

std::string BIOSAttrValTable::decodeStringEntry(
    const pldm_bios_attr_val_table_entry* entry, uint8_t /*strType*/)
{
    variable_field currentString{};
    pldm_bios_table_attr_value_entry_string_decode_string(entry,
                                                          &currentString);
    return std::string(currentString.ptr,
                       currentString.ptr + currentString.length);
}

const pldm_bios_attr_val_table_entry* BIOSAttrValTable::constructStringEntry(
    Table& table, uint16_t attrHandle, uint8_t attrType, const std::string& str)
{
    auto strLen = str.size();
    auto entryLength =
        pldm_bios_table_attr_value_entry_encode_string_length(strLen);
    auto tableSize = table.size();
    table.resize(tableSize + entryLength);
    pldm_bios_table_attr_value_entry_encode_string(
        table.data() + tableSize, entryLength, attrHandle, attrType, strLen,
        str.c_str());
    return reinterpret_cast<pldm_bios_attr_val_table_entry*>(table.data() +
                                                             tableSize);
}

} // namespace bios
} // namespace responder
} // namespace pldm
