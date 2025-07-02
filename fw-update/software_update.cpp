#include "software_update.hpp"

#include "device_dedicated_updater.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;
namespace pldm::fw_update
{

using Unavailable = sdbusplus::xyz::openbmc_project::Common::Error::Unavailable;

using namespace phosphor::logging;

SoftwareUpdate::SoftwareUpdate(
    sdbusplus::bus_t& bus, const std::string& path,
    DeviceDedicatedUpdater& updater,
    const std::set<RequestedApplyTimes>& allowedApplyTimes) :
    sdbusplus::xyz::openbmc_project::Software::server::Update(
        bus, path.c_str()),
    updater(updater), allowedApplyTimes(allowedApplyTimes), softwarePath(path)
{}

sdbusplus::message::object_path
    SoftwareUpdate::startUpdate(sdbusplus::message::unix_fd image,
                                RequestedApplyTimes applyTime)
{
    debug("Requesting Image update with {FD}", "FD", image.fd);

    if (updater.isUpdateInProgress)
    {
        error("An update is already in progress for the device");
        report<Unavailable>();
        return softwarePath;
    }

    updater.isUpdateInProgress = true;

    // check if the apply time is allowed by our device
    if (!allowedApplyTimes.contains(applyTime))
    {
        error(
            "the selected apply time {APPLYTIME} is not allowed by the device",
            "APPLYTIME", applyTime);
        updater.isUpdateInProgress = false;
        report<Unavailable>();
        return softwarePath;
    }

    updater.activation->activation(SoftwareActivation::Activations::NotReady);

    info("Sending init update event for EID {EID}, image fd {FD}, apply time {APPLYTIME}",
         "EID", updater.eid, "FD", image.fd, "APPLYTIME", applyTime);
    updater.sendInitUpdateEvent(
        std::move(image), applyTime);

    return softwarePath;
}

} // namespace pldm::fw_update
