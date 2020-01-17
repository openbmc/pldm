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

BIOSStringTable::BIOSStringTable(const char* filePath) : BIOSTable(filePath)
{
    if (!isEmpty())
    {
        load(stringTable);
    }
}

std::string BIOSStringTable::findString(uint16_t handle) const
{
    auto stringEntry = pldm_bios_table_string_find_by_handle(
        stringTable.data(), stringTable.size(), handle);
    if (stringEntry == nullptr)
    {
        throw std::invalid_argument("Invalid String Handle");
    }
    auto strLength =
        pldm_bios_table_string_entry_decode_string_length(stringEntry);
    std::vector<char> buffer(strLength + 1 /* sizeof '\0' */);
    pldm_bios_table_string_entry_decode_string(stringEntry, buffer.data(),
                                               buffer.size());

    return std::string(buffer.data(), buffer.data() + strLength);
}

} // namespace bios
} // namespace responder
} // namespace pldm
