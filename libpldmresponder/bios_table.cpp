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

uint16_t
    BIOSStringTable::decodeHandle(const pldm_bios_string_table_entry* entry)
{
    return pldm_bios_table_string_entry_decode_handle(entry);
}

std::string
    BIOSStringTable::decodeString(const pldm_bios_string_table_entry* entry)
{
    auto strLength = pldm_bios_table_string_entry_decode_string_length(entry);
    std::vector<char> buffer(strLength + 1 /* sizeof '\0' */);
    pldm_bios_table_string_entry_decode_string(entry, buffer.data(),
                                               buffer.size());
    return std::string(buffer.data(), buffer.data() + strLength);
}

} // namespace bios
} // namespace responder
} // namespace pldm
