#include "effecters.hpp"

#include <map>

namespace pldm
{

namespace responder
{

namespace effecter
{

Id nextId()
{
    static Id id = 0;
    return ++id;
}

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

const DbusObj& get(Id id)
{
    return internal::idToDbus.at(id);
}

} // namespace dbus_mapping

} // namespace effecter
} // namespace responder
} // namespace pldm
