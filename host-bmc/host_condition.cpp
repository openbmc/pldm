#include "host_condition.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <string>

using namespace sdbusplus::xyz::openbmc_project::Common::Error;

namespace server = sdbusplus::xyz::openbmc_project::Condition::server;

namespace pldm
{
namespace dbus_api
{

void Host::updateCurrentFirmwareCondition(std::string& firmwareCondition)
{
    if (firmwareCondition.empty())
    {
        firmC = HostFirmware::FirmwareCondition::Unknown;
    }
    else
    {
        firmC = server::HostFirmware::convertFirmwareConditionFromString(
            firmwareCondition);
    }
    server::HostFirmware::currentFirmwareCondition(firmC);
}
} // namespace dbus_api
} // namespace pldm
