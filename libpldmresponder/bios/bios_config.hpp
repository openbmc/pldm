#pragma once

#include "bios_string_attribute.hpp"
#include "bios_table.hpp"

#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
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

    explicit BIOSConfig(const fs::path& jsonDir, const fs::path& tableDir);

    void setAttrValue(const void* entry, size_t size);

    Table getBIOSStringTable();
    Table getBIOSAttrTable();
    Table getBIOSAttrValueTable();

  private:
    std::vector<std::unique_ptr<BIOSAttribute>> biosAttributes;
    using ParseHandler = std::function<void(const Json& entry)>;
    void load(const fs::path& filePath, ParseHandler handler);
    const fs::path jsonDir;
    const fs::path tableDir;

    Table buildStringTable();
    Table constructStringEntries(const std::set<std::string>& strings);

    void buildAttrTables(
        const Table& stringTable); // build attr table and attr value table
    void constructAttrTablesEntries();

    void appendPadAndCheckSum(Table& table);

    void storeBIOSTable(const fs::path& path, const Table& table);
    Table loadBIOSTable(const fs::path& path);
};

} // namespace wip
} // namespace bios
} // namespace responder
} // namespace pldm