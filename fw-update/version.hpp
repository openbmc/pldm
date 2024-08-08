#pragma once

#include "xyz/openbmc_project/Software/Version/server.hpp"

#include <sdbusplus/bus.hpp>

namespace pldm::fw_update
{
using SoftwareVersionIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::Version>;

class Version : public SoftwareVersionIntf
{
  public:
    Version(sdbusplus::bus::bus& bus, const std::string& path,
            const std::string& versionString, VersionPurpose versionPurpose) :
        SoftwareVersionIntf(bus, path.c_str(), action::emit_interface_added)
    {
        purpose(versionPurpose);
        version(versionString);
        emit_object_added();
    }
};

} // namespace pldm::fw_update
