#pragma once

#include "bios_table.h"

#include "bios_attribute.hpp"
#include "bios_table.hpp"
#include "pldmd/dbus_impl_requester.hpp"

#include <nlohmann/json.hpp>

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace pldm
{
namespace responder
{
namespace bios
{

enum class BoundType
{
    LowerBound,
    UpperBound,
    ScalarIncrement,
    MinStringLength,
    MaxStringLength,
    OneOf
};

using AttributeName = std::string;
using AttributeType = std::string;
using ReadonlyStatus = bool;
using DisplayName = std::string;
using Description = std::string;
using MenuPath = std::string;
using CurrentValue = std::variant<int64_t, std::string>;
using DefaultValue = std::variant<int64_t, std::string>;
using OptionString = std::string;
using OptionValue = std::variant<int64_t, std::string>;
using Option = std::vector<std::tuple<OptionString, OptionValue>>;
using BIOSTableObj =
    std::tuple<AttributeType, ReadonlyStatus, DisplayName, Description,
               MenuPath, CurrentValue, DefaultValue, Option>;
using BaseBIOSTable = std::map<AttributeName, BIOSTableObj>;

using PendingObj = std::tuple<AttributeType, CurrentValue>;
using PendingAttributes = std::map<AttributeName, PendingObj>;

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
     *  @param[in] fd - socket descriptor to communicate to host
     *  @param[in] eid - MCTP EID of host firmware
     *  @param[in] requester - pointer to Requester object
     */
    explicit BIOSConfig(const char* jsonDir, const char* tableDir,
                        DBusHandler* const dbusHandler, int fd, uint8_t eid,
                        dbus_api::Requester* requester);

    /** @brief Set attribute value on dbus and attribute value table
     *  @param[in] entry - attribute value entry
     *  @param[in] size - size of the attribute value entry
     *  @param[in] updateBaseBIOSTable - update BaseBIOSTable D-Bus property
     *                                   if this is set to true
     *  @return pldm_completion_codes
     */
    int setAttrValue(const void* entry, size_t size,
                     bool updateBaseBIOSTable = true);

    /** @brief Remove the persistent tables */
    void removeTables();

    /** @brief Build bios tables(string,attribute,attribute value table)*/
    void buildTables();

    /** @brief Get BIOS table of specified type
     *  @param[in] tableType - The table type
     *  @return The bios table, std::nullopt if the table is unaviliable
     */
    std::optional<Table> getBIOSTable(pldm_bios_table_types tableType);

    /** @brief set BIOS table
     *  @param[in] tableType - Indicates what table is being transferred
     *             {BIOSStringTable=0x0, BIOSAttributeTable=0x1,
     *              BIOSAttributeValueTable=0x2}
     *  @param[in] table - table data
     *  @param[in] updateBaseBIOSTable - update BaseBIOSTable D-Bus property
     *                                   if this is set to true
     *  @return pldm_completion_codes
     */
    int setBIOSTable(uint8_t tableType, const Table& table,
                     bool updateBaseBIOSTable = true);

  private:
    const fs::path jsonDir;
    const fs::path tableDir;
    DBusHandler* const dbusHandler;
    BaseBIOSTable baseBIOSTableMaps;

    /** @brief socket descriptor to communicate to host */
    int fd;

    /** @brief MCTP EID of host firmware */
    uint8_t eid;

    /** @brief pointer to Requester object, primarily used to access API to
     *  obtain PLDM instance id.
     */
    dbus_api::Requester* requester;

    // vector persists all attributes
    using BIOSAttributes = std::vector<std::unique_ptr<BIOSAttribute>>;
    BIOSAttributes biosAttributes;

    using propName = std::string;
    using DbusChObjProperties = std::map<propName, PropertyValue>;

    // vector to catch the D-Bus property change signals for BIOS attributes
    std::vector<std::unique_ptr<sdbusplus::bus::match::match>> biosAttrMatch;

    /** @brief Method to update a BIOS attribute when the corresponding Dbus
     *  property is changed
     *  @param[in] chProperties - list of properties which have changed
     *  @param[in] biosAttrIndex - Index of BIOSAttribute pointer in
     * biosAttributes
     *  @return - none
     */
    void processBiosAttrChangeNotification(
        const DbusChObjProperties& chProperties, uint32_t biosAttrIndex);

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
            auto biosAttrIndex = biosAttributes.size() - 1;
            auto dBusMap = biosAttributes[biosAttrIndex]->getDBusMap();

            if (dBusMap.has_value())
            {
                using namespace sdbusplus::bus::match::rules;
                biosAttrMatch.push_back(
                    std::make_unique<sdbusplus::bus::match::match>(
                        pldm::utils::DBusHandler::getBus(),
                        propertiesChanged(dBusMap->objectPath,
                                          dBusMap->interface),
                        [this,
                         biosAttrIndex](sdbusplus::message::message& msg) {
                            DbusChObjProperties props;
                            std::string iface;
                            msg.read(iface, props);
                            processBiosAttrChangeNotification(props,
                                                              biosAttrIndex);
                        }));
            }
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

    /** @brief Helper function to parse json
     *  @param[in] filePath - Path of json file
     *  @param[in] handler - Handler to process each entry in the json
     */
    void load(const fs::path& filePath, ParseHandler handler);

    /** @brief Build String Table and persist it
     *  @return The built string table, std::nullopt if it fails.
     */
    std::optional<Table> buildAndStoreStringTable();

    /** @brief Build attr table and attr value table and persist them
     *  @param[in] stringTable - The string Table
     */
    void buildAndStoreAttrTables(const Table& stringTable);

    /** @brief Persist the table
     *  @param[in] path - Path to persist the table
     *  @param[in] table - The table
     */
    void storeTable(const fs::path& path, const Table& table);

    /** @brief Load bios table to ram
     *  @param[in] path - Path of the table
     *  @return The table, std::nullopt if loading fails
     */
    std::optional<Table> loadTable(const fs::path& path);

    /** @brief Check the attribute value to update
     *  @param[in] attrValueEntry - The attribute value entry to update
     *  @param[in] attrEntry - The attribute table entry
     *  @param[in] stringTable - The string  table
     *  @return pldm_completion_codes
     */
    int checkAttrValueToUpdate(
        const pldm_bios_attr_val_table_entry* attrValueEntry,
        const pldm_bios_attr_table_entry* attrEntry, Table& stringTable);

    /** @brief Check the attribute table
     *  @param[in] table - The table
     *  @return pldm_completion_codes
     */
    int checkAttributeTable(const Table& table);

    /** @brief Check the attribute value table
     *  @param[in] table - The table
     *  @return pldm_completion_codes
     */
    int checkAttributeValueTable(const Table& table);

    /** @brief Update the BaseBIOSTable property of the D-Bus interface
     */
    void updateBaseBIOSTableProperty();

    /** @brief Listen the PendingAttributes property of the D-Bus interface and
     *         update BaseBIOSTable
     */
    void listenPendingAttributes();

    /** @brief Find attribute handle from bios attribute table
     *  @param[in] attrName - attribute name
     *  @return attribute handle
     */
    uint16_t findAttrHandle(const std::string& attrName);

    /** @brief Listen the PendingAttributes property of the D-Bus interface
     * and update BaseBIOSTable
     *  @param[in] msg - Data associated with subscribed signal
     */
    void constructPendingAttribute(const PendingAttributes& pendingAttributes);
};

} // namespace bios
} // namespace responder
} // namespace pldm
