#include "update.hpp"

#include "update_manager.hpp"

namespace pldm
{
namespace fw_update
{

sdbusplus::object_path Update::startUpdate(
    sdbusplus::message::unix_fd image,
    ApplyTimeIntf::RequestedApplyTimes applyTime [[maybe_unused]])
{
    namespace software = sdbusplus::xyz::openbmc_project::Software::server;
    // If a firmware activation of a package is in progress, don't proceed with
    // package processing
    if (updateManager->activation)
    {
        if (updateManager->activation->activation() ==
            software::Activation::Activations::Activating)
        {
            throw sdbusplus::xyz::openbmc_project::Common::Error::Unavailable();
        }
        else
        {
            updateManager->resetActivationState();
        }
    }

    info("Starting update for image {FD}", "FD", image.fd);
    return sdbusplus::object_path(updateManager->processFd(image.fd));
}

} // namespace fw_update
} // namespace pldm
