#include "update.hpp"

#include "update_manager.hpp"

#include <sys/stat.h>

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

    struct stat st;
    if (fstat(image, &st) < 0)
    {
        throw std::system_error(errno, std::generic_category(),
                                "Failed to get file size");
    }

    if (st.st_size == 0)
    {
        throw sdbusplus::error::xyz::openbmc_project::software::update::
            InvalidImage();
    }

    auto holder = std::make_shared<MappedStreamHolder>(image, st.st_size);
    updateManager->setStreamHolder(holder);
    return sdbusplus::message::object_path(
        updateManager->processStream(holder->stream(), st.st_size));
}

} // namespace fw_update
} // namespace pldm
