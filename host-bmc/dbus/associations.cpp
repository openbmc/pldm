#include "associations.hpp"

namespace pldm
{
namespace dbus
{

AssociationsObj Associations::associations() const
{
    return sdbusplus::xyz::openbmc_project::Association::server::Definitions::
        associations();
}

AssociationsObj Associations::associations(AssociationsObj value)
{
    return sdbusplus::xyz::openbmc_project::Association::server::Definitions::
        associations(value);
}

} // namespace dbus
} // namespace pldm
