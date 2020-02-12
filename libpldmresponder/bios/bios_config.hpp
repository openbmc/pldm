#pragma once

#include "bios_attribute.hpp"
#include "bios_table.hpp"

#include <functional>
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

using BIOSAttributes = std::vector<std::unique_ptr<BIOSAttribute>>;

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

    int setAttrValue(const void* entry, size_t size);

    std::optional<Table> getBIOSStringTable();
    std::optional<Table> getBIOSAttrTable();
    std::optional<Table> getBIOSAttrValueTable();

  private:
    BIOSAttributes biosAttributes;
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