#include "bios_table.hpp"

#include <fstream>

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

} // namespace bios
} // namespace responder
} // namespace pldm
