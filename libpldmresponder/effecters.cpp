#include "effecters.hpp"

#include <map>

namespace pldm
{

namespace responder
{

namespace effecter
{

namespace dbus_mapping
{

namespace internal
{

std::map<Id, Paths> idToDbus{};

} // namespace internal

void add(Id id, Paths&& paths)
{
    internal::idToDbus.emplace(id, std::move(paths));
}

Paths get(Id id)
{
    return internal::idToDbus.at(id);
}

} // namespace dbus_mapping

} // namespace effecter
} // namespace responder
} // namespace pldm
