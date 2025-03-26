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
    if (updateManager->activation)
    {
        updateManager->clearActivationInfo();
    }
    info("Starting update for image {FD}", "FD", static_cast<int>(image));
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

    // Process the stream directly
    return sdbusplus::message::object_path(updateManager->processStreamDefer(
        imageStream, imageStream.str().size()));
}

} // namespace fw_update
} // namespace pldm
