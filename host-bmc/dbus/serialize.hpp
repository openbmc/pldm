#pragma once

#include "type.hpp"

#include <libpldm/pdr.h>

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>

#include <filesystem>
#include <fstream>

namespace pldm
{
namespace serialize
{
namespace fs = std::filesystem;

using ObjectPath = fs::path;
using ObjectPathMaps = std::map<ObjectPath, pldm_entity_node*>;
using namespace pldm::dbus;

/** @class Serialize
 *  @brief This function is used for Store and restore the data
 */
class Serialize // final
{
  private:
    Serialize()
    {
        deserialize();
    }

  public:
    /** @brief Single object is required to handle store/retrieve data
     *       so, restricting creation/copying
     *
     */
    Serialize(const Serialize&) = delete;
    Serialize(Serialize&&) = delete;
    Serialize& operator=(const Serialize&) = delete;
    Serialize& operator=(Serialize&&) = delete;
    ~Serialize() = default;

    /** @brief Managing single static object throughout the process
     *
     * @returns singletone object to manage data
     */
    static Serialize& getSerialize()
    {
        static Serialize serialize;
        return serialize;
    }

    /** @brief Handles state sensor event
     *
     *  @param[in] path - Dbus path
     *  @param[in] intf - Interface path over dbus
     *  @param[in] name - Property Name over interface
     *  @param[in] value - Property Value over interface
     */
    void serialize(const std::string& path, const std::string& intf,
                   const std::string& name = "",
                   dbus::PropertyValue value = {});

    /** @brief Save the data into map of Key and property value pair
     *
     *  @param[in] key - Key to store property value
     *  @param[in] Property - property value will be stored in map
     */
    void serializeKeyVal(const std::string& key, dbus::PropertyValue value);

    /** @brief This function is to remove all key and property values
     *         from binary file.
     */
    bool deserialize();

    /** @brief This function is to get all saved key pair values
     *  @return returning dbus objects which contain all property and values
     */
    dbus::SavedObjs getSavedObjs()
    {
        return savedObjs;
    }

    /** @brief This function is to get all saved key pair values
     *   @return returning map of property and values
     */
    std::map<std::string, pldm::dbus::PropertyValue> getSavedKeyVals()
    {
        return savedKeyVal;
    }

    /** @brief Updating property value and save data as serialized way
     *
     *  @param[in] types - Key to store property value
     */
    void reSerialize(const std::vector<uint16_t> types);

    /** @brief Storing types of entity
     *
     *  @param[in] storeEntities - Map of Dbus objects mappings
     */
    void setEntityTypes(const std::set<uint16_t>& storeEntities);

    /** @brief This function to object path map which contain entity info.
     */
    void setObjectPathMaps(const ObjectPathMaps& maps);

    /** @brief This function added to change file location for unit test only.
     *
     */
    void setfilePathForUnitTest(fs::path path)
    {
        filePath = path;
    }

  private:
    dbus::SavedObjs savedObjs;
    fs::path filePath{PERSISTENT_FILE};
    std::set<uint16_t> storeEntityTypes;
    std::map<ObjectPath, pldm_entity> entityPathMaps;
    std::map<std::string, pldm::dbus::PropertyValue> savedKeyVal;
};

} // namespace serialize
} // namespace pldm
