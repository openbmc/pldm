#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

using namespace std;

namespace pldm
{
namespace dbus
{

using AssociationsObj =
    std::vector<std::tuple<std::string, std::string, std::string>>;

using PropertyValue =
    std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t,
                 uint64_t, double, std::string, AssociationsObj>;

// eg: {{entity type,  {object path, {entity instance number, entity
// container
//      id, {interfaces, {property name, value}}}}}}
using SavedObjs = std::map<
    uint16_t,
    std::map<std::string,
             std::tuple<
                 uint16_t, uint16_t,
                 std::map<std::string, std::map<std::string, PropertyValue>>>>>;

} // namespace dbus
} // namespace pldm
