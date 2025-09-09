#include "item_based_update.hpp"

#include "item_based_update_manager.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <cstdlib>

namespace pldm::fw_update
{

sdbusplus::message::object_path ItemBasedUpdate::startUpdate(
    sdbusplus::message::unix_fd image,
    ApplyTimeIntf::RequestedApplyTimes /*applyTime*/)
{
    namespace software = sdbusplus::xyz::openbmc_project::Software::server;
    info("Starting update process with FD {FD}", "FD", image.fd);
    if (updateManager->activation)
    {
        if (updateManager->activation->activation() ==
            software::Activation::Activations::Activating)
        {
            error("Update with package FD {FD} is already in progress", "FD",
                  image.fd);
            throw sdbusplus::xyz::openbmc_project::Common::Error::Unavailable();
        }
        else
        {
            updateManager->clearActivationInfo();
        }
    }

    return updateManager->processFd(image.fd);
}

} // namespace pldm::fw_update
