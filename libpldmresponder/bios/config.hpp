#pragma once

#include "attribute.hpp"
#include "table.hpp"

#include <functional>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "bios_table.h"

namespace pldm
{
namespace responder
{
namespace bios
{
namespace wip
{

class BIOSConfig
{
  public:
    BIOSConfig() = delete;
    BIOSConfig(const BIOSConfig&) = delete;
    BIOSConfig(BIOSConfig&&) = delete;
    BIOSConfig& operator=(const BIOSConfig&) = delete;
    BIOSConfig& operator=(BIOSConfig&&) = delete;
    ~BIOSConfig() = default;

    explicit BIOSConfig(const char* jsonDir, const char* tableDir);

    int setAttrValue(const void* entry, size_t size);

    void removeTables();

    void buildTables();

    std::optional<Table> getBIOSTable(pldm_bios_table_types tableType);

  private:
    using BIOSAttributes = std::vector<std::unique_ptr<BIOSAttribute>>;
    BIOSAttributes biosAttributes;
    template <typename T>
    void constructAttributes(const Json& entry)
    {
        try
        {
            biosAttributes.push_back(std::make_unique<T>(entry));
        }
        catch (const std::exception& e)
        {
            std::cerr << "Constructs Attribute Error, " << e.what()
                      << std::endl;
        }
    }
    using ParseHandler = std::function<void(const Json& entry)>;
    void load(const fs::path& filePath, ParseHandler handler);
    const fs::path jsonDir;
    const fs::path tableDir;

    std::optional<Table> buildStringTable();
    Table constructStringEntries(const std::set<std::string>& strings);

    void buildAttrTables(
        const Table& stringTable); // build attr table and attr value table
    void constructAttrTablesEntries();

    void appendPadAndCheckSum(Table& table);

    void storeBIOSTable(const fs::path& path, const Table& table);
    std::optional<Table> loadBIOSTable(const fs::path& path);
};

} // namespace wip
} // namespace bios
} // namespace responder
} // namespace pldm