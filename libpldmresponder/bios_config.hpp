#pragma once

#include "bios_attribute.hpp"
#include "bios_table.hpp"

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

/** @class BIOSConfig
 *  @brief Manager BIOS Attributes
 */
class BIOSConfig
{
  public:
    BIOSConfig() = delete;
    BIOSConfig(const BIOSConfig&) = delete;
    BIOSConfig(BIOSConfig&&) = delete;
    BIOSConfig& operator=(const BIOSConfig&) = delete;
    BIOSConfig& operator=(BIOSConfig&&) = delete;
    ~BIOSConfig() = default;

    /** @brief Construct BIOSConfig
     *  @param[in] jsonDir - The directory where json file exists
     *  @param[in] tableDir - The directory where the persistent table is placed
     *  @param[in] dbusHandler - Dbus Handler
     */
    explicit BIOSConfig(const char* jsonDir, const char* tableDir,
                        DBusHandler* const dbusHandler);

    /** @brief Set attribute value on dbus and attribute value table
     *  @param[in] entry - attribute value entry
     *  @param[in] size - size of the attribute value entry
     *  @return pldm_completion_codes
     */
    int setAttrValue(const void* entry, size_t size);

    /** @brief Remove the persistent tables */
    void removeTables();

    /** @brief Build bios tables(string,attribute,attribute value table)*/
    void buildTables();

    /** @brief Get BIOS table of specified type
     *  @param[in] tableType - The table type
     *  @return The bios table, std::nullopt if the table is unaviliable
     */
    std::optional<Table> getBIOSTable(pldm_bios_table_types tableType);

  private:
    const fs::path jsonDir;
    const fs::path tableDir;
    DBusHandler* const dbusHandler;

    // vector persists all attributes
    using BIOSAttributes = std::vector<std::unique_ptr<BIOSAttribute>>;
    BIOSAttributes biosAttributes;

    /** @brief Construct an attribute and persist it
     *  @tparam T - attribute type
     *  @param[in] entry - json entry
     */
    template <typename T>
    void constructAttribute(const Json& entry)
    {
        try
        {
            biosAttributes.push_back(std::make_unique<T>(entry, dbusHandler));
        }
        catch (const std::exception& e)
        {
            std::cerr << "Constructs Attribute Error, " << e.what()
                      << std::endl;
        }
    }

    /** Construct attributes and persist them */
    void constructAttributes();

    using ParseHandler = std::function<void(const Json& entry)>;
    void load(const fs::path& filePath, ParseHandler handler);

    std::optional<Table> buildStringTable();

    /** @brief build attr table and attr value table */
    void buildAttrTables(const Table& stringTable);

    void storeBIOSTable(const fs::path& path, const Table& table);

    std::optional<Table> loadBIOSTable(const fs::path& path);
};

} // namespace bios
} // namespace responder
} // namespace pldm