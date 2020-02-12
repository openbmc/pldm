#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace pldm
{

namespace responder
{

namespace effecter
{

using Id = uint16_t;

/** @brief Get next available id to assign to an effecter
 *
 *  @return  uint16_t - effecter id
 */
Id nextId();

namespace dbus_mapping
{

struct DBusMapping
{
    std::string objectPath;   //!< D-Bus object path
    std::string interface;    //!< D-Bus interface
    std::string propertyName; //!< D-Bus property name
};

using DbusObjs = std::vector<DBusMapping>;

/** @brief Add an effecter id -> D-Bus objects mapping
 *
 *  @param[in] id - effecter id
 *  @param[in] dbusObj - list of D-Bus object structure
 */
void add(Id id, DbusObjs&& dbusObj);

/** @brief Retrieve an effecter id -> D-Bus objects mapping
 *
 *  @param[in] id - effecter id
 *
 *  @return DbusObjs - list of D-Bus object structure
 */
const DbusObjs& get(Id id);
} // namespace dbus_mapping

} // namespace effecter
} // namespace responder
} // namespace pldm
