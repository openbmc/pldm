#pragma once

#include "xyz/openbmc_project/Software/Version/server.hpp"
#include <sdbusplus/bus.hpp>

#include <string>

namespace pldm
{

namespace fw_update
{


using VersionIntf = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::Version>;

/** @class Version
 *
 *  Concrete implementation of the xyz.openbmc_project.Software.Version
 *  D-Bus interface to represent the ActiveComponentVersionString of the
 *  components discovered by the PLDM FW update inventory commands.
 */
class Version : public VersionIntf
{
  public:

    Version() = delete;
    ~Version() = default;
    Version(const Version&) = delete;
    Version& operator=(const Version&) = delete;
    Version(Version&&) = default;
    Version& operator=(Version&&) = default;

    /** @brief Constructs Version Software Manager
     *
     * @param[in] bus - reference to the system bus
     * @param[in] objPath - D-Bus object path
     * @param[in] version - version string
     * @param[in] purpose - version purpose
     */
    Version(sdbusplus::bus::bus& bus, const std::string& objPath,
            const std::string& version, VersionPurpose purpose) :
        VersionIntf(bus, (objPath).c_str(), true)
    {
        VersionIntf::version(version, true);
        VersionIntf::purpose(purpose, true);
        VersionIntf::emit_object_added();
    }
};

} // namespace fw_update

} // namespace pldm