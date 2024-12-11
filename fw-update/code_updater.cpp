#include "code_updater.hpp"

#include <phosphor-logging/lg2.hpp>

#include <sstream>

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

sdbusplus::message::object_path CodeUpdater::startUpdate(
    sdbusplus::message::unix_fd image,
    ApplyTimeIntf::RequestedApplyTimes applyTime [[maybe_unused]])
{
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

    updateManager->processStream(imageStream, imageStream.str().size());

    return sdbusplus::message::object_path(objPath);
}

} // namespace pldm::fw_update
