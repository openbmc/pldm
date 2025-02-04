#include "availability.hpp"

namespace pldm
{
namespace dbus
{

bool Availability::available() const
{
    return sdbusplus::xyz::openbmc_project::State::Decorator::server::
        Availability::available();
}

bool Availability::available(bool value)
{
    return sdbusplus::xyz::openbmc_project::State::Decorator::server::
        Availability::available(value);
}

} // namespace dbus
} // namespace pldm
