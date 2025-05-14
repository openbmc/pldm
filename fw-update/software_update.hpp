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
    SoftwareUpdate(sdbusplus::async::context& ctx, const char* path,
                   DeviceDedicatedUpdater& updater,
                   const std::set<RequestedApplyTimes>& allowedApplyTimes = {
                       RequestedApplyTimes::Immediate});

    auto method_call(start_update_t su, auto image, auto applyTime)
        -> sdbusplus::async::task<start_update_t::return_type>;

    auto get_property(
        sdbusplus::aserver::xyz::openbmc_project::software::details::Update<
            pldm::fw_update::SoftwareUpdate,
            sdbusplus::async::server::server<
                pldm::fw_update::SoftwareUpdate,
                sdbusplus::aserver::xyz::openbmc_project::software::details::
                    Update>>::allowed_apply_times_t aat) const
        -> std::set<RequestedApplyTimes>;
    ;

  private:
    DeviceDedicatedUpdater& updater;

    const std::set<RequestedApplyTimes> allowedApplyTimes;
};

} // namespace pldm::fw_update
