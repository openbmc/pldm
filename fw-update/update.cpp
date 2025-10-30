#include "update.hpp"

#include "update_manager.hpp"

namespace pldm
{
namespace fw_update
{

sdbusplus::message::object_path Update::startUpdate(
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
    char buffer[4096];
    ssize_t bytesRead;
    imageStream.str(std::string());

    while ((bytesRead = read(image, buffer, sizeof(buffer))) > 0)
    {
        imageStream.write(buffer, bytesRead);
    }

    if (bytesRead < 0)
    {
        throw std::runtime_error("Failed to read image file descriptor");
    }

    return sdbusplus::message::object_path(updateManager->processStreamDefer(
        imageStream, imageStream.str().size()));
}

} // namespace fw_update
} // namespace pldm
