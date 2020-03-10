#include "bios_attribute.hpp"

#include "utils.hpp"

#include <iostream>
#include <variant>

namespace pldm
{
namespace responder
{
namespace bios
{

BIOSAttribute::BIOSAttribute(const Json& entry,
                             DBusHandler* const dbusHandler) :
    name(entry.at("attribute_name")),
    readOnly(!entry.contains("dbus")), dbusHandler(dbusHandler)
{
    if (!readOnly)
    {
        std::string objectPath = entry.at("dbus").at("object_path");
        std::string interface = entry.at("dbus").at("interface");
        std::string propertyName = entry.at("dbus").at("property_name");
        std::string propertyType = entry.at("dbus").at("property_type");

        dBusMap = {objectPath, interface, propertyName, propertyType};
    }
}

} // namespace bios
} // namespace responder
} // namespace pldm
