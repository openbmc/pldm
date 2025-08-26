#pragma once

#include "update.hpp"

#include <xyz/openbmc_project/Software/ApplyTime/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

namespace pldm
{

namespace fw_update
{

class ItemBasedUpdateManager;

using UpdateIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::Update>;
using ApplyTimeIntf =
    sdbusplus::xyz::openbmc_project::Software::server::ApplyTime;

/** @class Update
 *
 *  Concrete implementation of xyz.openbmc_project.Software.Update D-Bus
 *  interface
 */
class ItemBasedUpdate : public UpdateIntf
{
  public:
    /** @brief Constructor
     *
     *  @param[in] bus - Bus to attach to
     *  @param[in] objPath - D-Bus object path
     *  @param[in] updateManager - Reference to FW update manager
     */
    ItemBasedUpdate(sdbusplus::bus::bus& bus, const std::string& path,
           ItemBasedUpdateManager* updateManager) :
        UpdateIntf(bus, path.c_str()), updateManager(updateManager),
        objPath(path)
    {}

    virtual sdbusplus::message::object_path startUpdate(
        sdbusplus::message::unix_fd image,
        ApplyTimeIntf::RequestedApplyTimes applyTime) override;

    ~ItemBasedUpdate() noexcept override = default;

  private:
    ItemBasedUpdateManager* updateManager;
    const std::string objPath;
    std::stringstream imageStream;
};

} // namespace fw_update

} // namespace pldm
