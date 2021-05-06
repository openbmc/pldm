#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Condition/HostFirmware/server.hpp>

namespace pldm
{
namespace dbus_api
{

using HostIntf = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Condition::server::HostFirmware>;

class Host : public HostIntf
{
  public:
    Host() = delete;
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;
    Host(Host&&) = delete;
    Host& operator=(Host&&) = delete;
    virtual ~Host() = default;

    Host(sdbusplus::bus::bus& bus, const std::string& path) :
        HostIntf(bus, path.c_str()){};

    void updateCurrentFirmwareCondition(std::string& firmwareCondition);

  private:
    sdbusplus::xyz::openbmc_project::Condition::server::HostFirmware::
        FirmwareCondition firmC;
};
} // namespace dbus_api
} // namespace pldm
