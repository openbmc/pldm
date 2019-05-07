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
using Paths = std::vector<std::string>;

/** @brief Add an effecter id -> DBus objects mapping
 *
 *  @param[in] id - effecter id
 *  @param[in] paths - list of DBus object paths
 */
void add(Id id, Paths&& paths);

/** @brief Retrieve an effecter id -> DBus objects mapping
 *
 *  @param[in] id - effecter id
 *
 *  @return Paths - list of DBus object paths
 */
Paths get(Id id);
} // namespace dbus_mapping

} // namespace effecter
} // namespace responder
} // namespace pldm
