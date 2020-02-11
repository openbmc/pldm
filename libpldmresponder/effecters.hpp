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

namespace dbus_mapping
{

struct DBusMapping
{
    std::string objectPath;   //!< D-Bus object path
    std::string interface;    //!< D-Bus interface
    std::string propertyName; //!< D-Bus property name
};

using DbusObj = std::vector<DBusMapping>;

/** @brief Add an effecter id -> D-Bus objects mapping
 *
 *  @param[in] id - effecter id
 *  @param[in] dbusObj - list of D-Bus object structure
 */
void add(Id id, DbusObj&& dbusObj);

/** @brief Retrieve an effecter id -> D-Bus objects mapping
 *
 *  @param[in] id - effecter id
 *
 *  @return DbusObj - list of D-Bus object structure
 */
DbusObj get(Id id);
} // namespace dbus_mapping

} // namespace effecter
} // namespace responder
} // namespace pldm
