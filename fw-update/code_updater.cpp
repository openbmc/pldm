#include "code_updater.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sstream>

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

sdbusplus::message::object_path
    CodeUpdater::startUpdate(sdbusplus::message::unix_fd image,
                             ApplyTimeIntf::RequestedApplyTimes /*applyTime*/)
{
    info("Starting update for image {FD}", "FD", static_cast<int>(image));

    updateManager->processFd(image);

    return sdbusplus::message::object_path(objPath);
}

} // namespace pldm::fw_update
                              