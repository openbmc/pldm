#include "effecters.hpp"

#include <map>

namespace pldm
{

namespace responder
{

namespace effecter
{
using namespace pldm::utils;

Id nextId()
{
    static Id id = 0;
    return ++id;
}

namespace dbus_mapping
{

namespace internal
{

std::map<Id, DbusObjs> idToDbusObjs{};

std::map<Id, DbusValMaps> idToDbusValMaps;

} // namespace internal

void addDbusObjs(Id id, DbusObjs&& dbusObjs)
{
    internal::idToDbusObjs.emplace(id, std::move(dbusObjs));
}

const DbusObjs& getDbusObjs(Id id)
{
    return internal::idToDbusObjs.at(id);
}

void addDbusValMaps(Id id, DbusValMaps&& dbusValMap)
{
    internal::idToDbusValMaps.emplace(id, std::move(dbusValMap));
}

const DbusValMaps& getDbusValMaps(Id id)
{
    return internal::idToDbusValMaps.at(id);
}

} // namespace dbus_mapping

} // namespace effecter
} // namespace responder
} // namespace pldm
