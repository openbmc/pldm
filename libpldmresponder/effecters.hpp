#pragma once

#include "pdr_utils.hpp"
#include "utils.hpp"

#include <stdint.h>

#include <string>
#include <vector>

namespace pldm
{

namespace responder
{

namespace effecter
{
using namespace pldm::utils;
using namespace pldm::responder::pdr_utils;

using Id = uint16_t;

/** @brief Get next available id to assign to an effecter
 *
 *  @return  uint16_t - effecter id
 */
Id nextId();

namespace dbus_mapping
{

using DbusObjs = std::vector<DBusMapping>;
using DbusValMaps = std::vector<DbusIdToValMap>;

/** @brief Add an effecter id -> D-Bus objects mapping
 *
 *  @param[in] id - effecter id
 *  @param[in] dbusObj - list of D-Bus object structure
 */
void addDbusObjs(Id id, DbusObjs&& dbusObjs);

/** @brief Retrieve an effecter id -> D-Bus objects mapping
 *
 *  @param[in] id - effecter id
 *
 *  @return DbusObjs - list of D-Bus object structure
 */
const DbusObjs& getDbusObjs(Id id);

/** @brief Add an effecter id -> D-Bus value mapping
 *
 *  @param[in] id - effecter id
 *  @param[in] dbusValMap - list of DBus property value to attribute value
 */
void addDbusValMaps(Id id, DbusValMaps&& dbusValMap);

/** @brief Retrieve an effecter id -> D-Bus value mapping
 *
 *  @param[in] id - effecter id
 *
 *  @return DbusValMaps - list of DBus property value to attribute value
 */
const DbusValMaps& getDbusValMaps(Id id);

} // namespace dbus_mapping

} // namespace effecter
} // namespace responder
} // namespace pldm
