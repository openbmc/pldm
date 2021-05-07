#include "host_condition.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <string>

using namespace sdbusplus::xyz::openbmc_project::Common::Error;

namespace server = sdbusplus::xyz::openbmc_project::Condition::server;

namespace pldm
{
namespace dbus_api
{

Host::FirmwareCondition Host::currentFirmwareCondition() const
{
    bool hostRunning = false;

    if (hostPdrObj != nullptr)
    {
        hostRunning = hostPdrObj.get()->isHostUp();
    }

    auto value = hostRunning ? Host::FirmwareCondition::Running
                             : Host::FirmwareCondition::Off;

    return value;
}

} // namespace dbus_api
} // namespace pldm
