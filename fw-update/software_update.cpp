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
    sdbusplus::async::context& ctx, const char* path,
    DeviceDedicatedUpdater& updater,
    const std::set<RequestedApplyTimes>& allowedApplyTimes) :
    sdbusplus::aserver::xyz::openbmc_project::software::Update<SoftwareUpdate>(
        ctx, path),
    updater(updater), allowedApplyTimes(allowedApplyTimes)
{}

auto SoftwareUpdate::method_call(start_update_t /*unused*/, auto /*image*/,
                                 auto /*applyTime*/)
    -> sdbusplus::async::task<start_update_t::return_type>
{
    // TODO: Implement the start_update method.
    co_return std::string{};
}

auto SoftwareUpdate::get_property(
    sdbusplus::aserver::xyz::openbmc_project::software::details::Update<
        pldm::fw_update::SoftwareUpdate,
        sdbusplus::async::server::server<
            pldm::fw_update::SoftwareUpdate,
            sdbusplus::aserver::xyz::openbmc_project::software::details::
                Update>>::allowed_apply_times_t /*unused*/) const
{
    return allowedApplyTimes;
}

} // namespace pldm::fw_update
