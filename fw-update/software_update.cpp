#include "software_update.hpp"

#include "device_dedicated_updater.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Software/Update/aserver.hpp>

PHOSPHOR_LOG2_USING;
namespace pldm::fw_update
{

using Unavailable = sdbusplus::xyz::openbmc_project::Common::Error::Unavailable;

using namespace phosphor::logging;

SoftwareUpdate::SoftwareUpdate(
    sdbusplus::async::context& ctx, const std::string& path,
    DeviceDedicatedUpdater& updater,
    const std::set<RequestedApplyTimes>& allowedApplyTimes) :
    sdbusplus::aserver::xyz::openbmc_project::software::Update<SoftwareUpdate>(
        ctx, path.c_str()),
    updater(updater), allowedApplyTimes(allowedApplyTimes), softwarePath(path)
{}

auto SoftwareUpdate::method_call(start_update_t /*unused*/, auto image,
                                 auto applyTime)
    -> sdbusplus::async::task<start_update_t::return_type>
{
    debug("Requesting Image update with {FD}", "FD", image.fd);
    // check if the apply time is allowed by our device
    if (!allowedApplyTimes.contains(applyTime))
    {
        error(
            "the selected apply time {APPLYTIME} is not allowed by the device",
            "APPLYTIME", applyTime);
        report<Unavailable>();
        co_return sdbusplus::message::object_path();
    }

    ctx.spawn([](sdbusplus::message::unix_fd& image,
                 DeviceDedicatedUpdater& updater,
                 RequestedApplyTimes applyTime) -> sdbusplus::async::task<> {
        co_await updater.initUpdate(image, applyTime);
        co_return;
    }(image, updater, applyTime));

    co_return softwarePath;
}

auto SoftwareUpdate::get_property(allowed_apply_times_t /*unused*/) const
    -> allowed_apply_times_t::value_type
{
    // Return the allowed apply times for this software update.
    return allowedApplyTimes;
}

} // namespace pldm::fw_update
