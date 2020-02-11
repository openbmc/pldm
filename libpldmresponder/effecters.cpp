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

std::map<Id, DbusObj> idToDbus{};

} // namespace internal

void add(Id id, DbusObj&& dbusObj)
{
    internal::idToDbus.emplace(id, std::move(dbusObj));
}

DbusObj get(Id id)
{
    return internal::idToDbus.at(id);
}

} // namespace dbus_mapping

} // namespace effecter
} // namespace responder
} // namespace pldm
