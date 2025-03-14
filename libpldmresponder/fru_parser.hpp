#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <set>
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
using Interfaces = std::set<Interface>;
using Property = std::string;
using PropertyType = std::string;
using EntityType = uint16_t;

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

// DBusLookupInfo contains info to lookup in the D-Bus inventory, D-Bus
// inventory service bus name, root path of the inventory D-Bus objects and list
//  of xyz.openbmc_project.Inventory.Item.* interface names.
using DBusLookupInfo =
    std::tuple<pldm::responder::dbus::Service, pldm::responder::dbus::RootPath,
               pldm::responder::dbus::Interfaces>;
using FieldInfo = std::tuple<
    pldm::responder::dbus::Interface, pldm::responder::dbus::Property,
    pldm::responder::dbus::PropertyType, pldm::responder::fru::FieldType>;

using FruRecordInfo =
    std::tuple<pldm::responder::fru::RecordType,
               pldm::responder::fru::EncodingType, std::vector<FieldInfo>>;
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
    FruParser() = delete;
    explicit FruParser(const std::string& dirPath,
                       const std::filesystem::path& fruMasterJsonPath);
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
    const FruRecordInfos& getRecordInfo(
        const pldm::responder::dbus::Interface& intf) const
    {
        return recordMap.at(intf);
    }

    pldm::responder::dbus::EntityType getEntityType(
        const pldm::responder::dbus::Interface& intf) const
    {
        return intfToEntityType.at(intf);
    }

  private:
    /** @brief Parse the FRU Configuration JSON file in the directory path
     *         except the FRU_Master.json and build the FRU record information
     *
     *  @param[in] dirPath - directory path where all the FRU configuration JSON
     *                       files exist
     */
    void setupFruRecordMap(const std::string& dirPath);

    /** @brief Set the default service root D-Bus path and the item interfaces.
     *
     *  @param[in] fruMasterJsonPath - json file path that contains the FRU
     * D-Bus lookup map
     */
    void setupDefaultDBusLookup(const std::filesystem::path& fruMasterJsonPath);

    /** @brief Build the default FRU record information
     */
    void setupDefaultFruRecordMap();

    std::optional<DBusLookupInfo> lookupInfo;
    FruRecordMap recordMap;
    std::map<pldm::responder::dbus::Interface,
             pldm::responder::dbus::EntityType>
        intfToEntityType;
};

} // namespace fru_parser

} // namespace responder

} // namespace pldm
