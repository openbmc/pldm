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

Id nextId();

namespace dbus_mapping
{
    using Paths = std::vector<std::string>;

    void add(Id id, Paths&& paths);
    Paths get(Id id);
} // namespace dbus_mapping

} // namespace effecter
} // namespace responder
} // namespace pldm
