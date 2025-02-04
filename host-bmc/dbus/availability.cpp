#include "availability.hpp"

#include "serialize.hpp"

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
    pldm::serialize::Serialize::getSerialize().serialize(path, "Available",
                                                         "available", value);

    return sdbusplus::xyz::openbmc_project::State::Decorator::server::
        Availability::available(value);
}

} // namespace dbus
} // namespace pldm
