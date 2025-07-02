#pragma once

#include <xyz/openbmc_project/Software/Update/server.hpp>

namespace pldm::fw_update
{

class DeviceDedicatedUpdater;

using RequestedApplyTimes = sdbusplus::common::xyz::openbmc_project::software::
    ApplyTime::RequestedApplyTimes;

using UpdateIntf = sdbusplus::xyz::openbmc_project::Software::server::Update;

class SoftwareUpdate : public UpdateIntf
{
  public:
    SoftwareUpdate(sdbusplus::bus_t& bus, const std::string& path,
                   DeviceDedicatedUpdater& updater,
                   const std::set<RequestedApplyTimes>& allowedApplyTimes = {
                       RequestedApplyTimes::Immediate});

    virtual sdbusplus::message::object_path startUpdate(
        sdbusplus::message::unix_fd image,
        RequestedApplyTimes applyTime) override;

  private:
    DeviceDedicatedUpdater& updater;

    const std::set<RequestedApplyTimes> allowedApplyTimes;

    const std::string& softwarePath;
};

} // namespace pldm::fw_update
