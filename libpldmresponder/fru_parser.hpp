#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace pldm
{

namespace responder
{

namespace dbus
{

using Service = std::string;
using RootPath = std::string;
using Interface = std::string;
using Interfaces = std::vector<Interface>;
using Property = std::string;
using PropertyType = std::string;

} // namespace dbus

namespace fru
{

using RecordType = uint8_t;
using EncodingType = uint8_t;
using FieldType = uint8_t;

} // namespace fru

namespace fru_parser
{

namespace fs = std::filesystem;
using namespace dbus;
using namespace fru;

using DBusLookupInfo = std::tuple<Service, RootPath, Interfaces>;
using FieldInfo = std::tuple<Interface, Property, PropertyType, FieldType>;

using FruRecordInfo =
    std::tuple<RecordType, EncodingType, std::vector<FieldInfo>>;
using FruRecordInfos = std::vector<FruRecordInfo>;

using ItemIntfName = std::string;
using FruRecordMap = std::map<ItemIntfName, FruRecordInfos>;

/** @class FruParser
 *
 *  @brief Parses the PLDM FRU configuration files to populate the data
 *         structure, containing the information needed to map the D-Bus
 *         inventory information into PLDM FRU Record.
 */
class FruParser
{

  public:
    FruParser() = default;
    virtual ~FruParser() = default;
    FruParser(const FruParser&) = default;
    FruParser& operator=(const FruParser&) = default;
    FruParser(FruParser&&) = default;
    FruParser& operator=(FruParser&&) = default;

    /** @brief Provides the service, root D-Bus path and the interfaces that is
     *         needed to build FRU record data table
     *
     *  @return service and inventory interfaces needed to build the FRU records
     */
    const DBusLookupInfo& inventoryLookup() const
    {
        return lookupInfo.value();
    }

    /** @brief Get the information need to create PLDM FRU records for a
     * inventory item type. The parameter to this API is the inventory item
     * type, for example xyz.openbmc_project.Inventory.Item.Cpu for CPU's
     *
     *  @param[in] intf - name of the item interface
     *
     *  @return return the info create the PLDM FRU records from inventory D-Bus
     *          objects
     */
    const FruRecordInfos& getRecordInfo(Interface intf) const
    {
        return recordMap.at(intf);
    }

    /** @brief Check if FRU parser config information is populated
     *
     *  @return bool - true if FRU parser config information is empty,
     *                 false otherwise
     */
    bool empty() const
    {
        return !lookupInfo.has_value() && recordMap.empty();
    }

    /** @brief Parse every BIOS Configuration JSON file in the directory path
     *
     *  @param[in] dirPath - directory path where all the FRU configuration JSON
     *                       files exist
     */
    void setupFRUInfo(const std::string& dirPath);

  private:
    /** @brief Parse the FRU_Master.json file and populate the D-Bus lookup
     *         information which provides the service, root D-Bus path and the
     *         item interfaces.
     *
     *  @param[in] filePath - file path to FRU_Master.json
     */
    void setupDBusLookup(const fs::path& filePath);

    /** @brief Parse the BIOS Configuration JSON file in the directory path
     *         except the FRU_Master.json and build the FRU record information
     *
     *  @param[in] dirPath - directory path where all the FRU configuration JSON
     *                       files exist
     */
    void setupFruRecordMap(const std::string& dirPath);

    std::optional<DBusLookupInfo> lookupInfo;
    FruRecordMap recordMap;
};

/** @brief Build (if not built already) the FRU parser config
 *
 *  @param[in] dir - directory containing platform specific FRU JSON files
 *
 *  @return FruParser& - Reference to instance of FruParser
 */
FruParser& get(const std::string& dir);

} // namespace fru_parser

} // namespace responder

} // namespace pldm
