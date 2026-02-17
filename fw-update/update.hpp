#pragma once

#include <xyz/openbmc_project/Software/ApplyTime/server.hpp>
#include <xyz/openbmc_project/Software/MultipartUpdate/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

namespace pldm
{

namespace fw_update
{

class UpdateManager;

using UpdateIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::Update,
    sdbusplus::xyz::openbmc_project::Software::server::MultipartUpdate>;
using ApplyTimeIntf =
    sdbusplus::xyz::openbmc_project::Software::server::ApplyTime;

/** @class Update
 *
 *  Concrete implementation of xyz.openbmc_project.Software.Update D-Bus
 *  interface
 */
class Update : public UpdateIntf
{
  public:
    /** @brief Constructor
     *
     *  @param[in] bus - Bus to attach to
     *  @param[in] objPath - D-Bus object path
     *  @param[in] updateManager - Reference to FW update manager
     */
    Update(sdbusplus::bus_t& bus, const std::string& path,
           UpdateManager* updateManager) :
        UpdateIntf(bus, path.c_str()), updateManager(updateManager),
        objPath(path)
    {}

    Update(const Update&) = delete;
    Update& operator=(const Update&) = delete;
    Update(Update&&) = delete;
    Update& operator=(Update&&) = delete;

    sdbusplus::message::object_path startUpdate(
        sdbusplus::message::unix_fd image,
        ApplyTimeIntf::RequestedApplyTimes applyTime) override;

    ~Update() noexcept override = default;

  private:
    UpdateManager* updateManager;
    const std::string objPath;
    std::stringstream imageStream;
};

} // namespace fw_update

} // namespace pldm
