#pragma once

#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Software/Update/aserver.hpp>

namespace pldm::fw_update
{

class DeviceDedicatedUpdater;

using RequestedApplyTimes = sdbusplus::common::xyz::openbmc_project::software::
    ApplyTime::RequestedApplyTimes;

class SoftwareUpdate :
    public sdbusplus::aserver::xyz::openbmc_project::software::Update<
        SoftwareUpdate>
{
  public:
    SoftwareUpdate(sdbusplus::async::context& ctx, const std::string& path,
                   DeviceDedicatedUpdater& updater,
                   const std::set<RequestedApplyTimes>& allowedApplyTimes = {
                       RequestedApplyTimes::Immediate});

    auto method_call(start_update_t su, auto image, auto applyTime)
        -> sdbusplus::async::task<start_update_t::return_type>;

    auto get_property(allowed_apply_times_t aat) const
        -> allowed_apply_times_t::value_type;

  private:
    [[maybe_unused]] DeviceDedicatedUpdater& updater;

    const std::set<RequestedApplyTimes> allowedApplyTimes;

    const std::string& softwarePath;
};

} // namespace pldm::fw_update
