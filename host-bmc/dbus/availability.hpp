#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/State/Decorator/Availability/server.hpp>

#include <string>

namespace pldm
{
namespace dbus
{
using AvailabilityIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::State::Decorator::server::Availability>;

class Availability : public AvailabilityIntf
{
  public:
    Availability() = delete;
    ~Availability() = default;
    Availability(const Availability&) = delete;
    Availability& operator=(const Availability&) = delete;
    Availability(Availability&&) = delete;
    Availability& operator=(Availability&&) = delete;

    Availability(sdbusplus::bus_t& bus, const std::string& objPath) :
        AvailabilityIntf(bus, objPath.c_str()), path(objPath)
    {}

    /** Get value of Available */
    bool available() const override;

    /** Set value of Available */
    bool available(bool value) override;

  private:
    std::string path;
};

} // namespace dbus
} // namespace pldm
