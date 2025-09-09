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
    auto packageFd = image.fd;
    struct stat st;
    if (fstat(packageFd, &st) < 0)
    {
        error("Failed to get package file status for FD {FD}", "FD", packageFd);
        throw sdbusplus::xyz::openbmc_project::Common::Error::Unavailable();
    }
    size_t packageSize = st.st_size;

    auto packageMap = static_cast<uint8_t*>(
        mmap(nullptr, packageSize, PROT_READ, MAP_PRIVATE, packageFd, 0));
    if (packageMap == MAP_FAILED)
    {
        error("Failed to map package file for FD {FD}", "FD", packageFd);
        throw sdbusplus::xyz::openbmc_project::Common::Error::Unavailable();
    }

    package = std::span<uint8_t>(packageMap, packageSize);

    info("Sending package with size {SIZE} for FD {FD}", "SIZE", packageSize,
         "FD", packageFd);
    return updateManager->sendProcessPackage(package);
}

} // namespace pldm::fw_update
