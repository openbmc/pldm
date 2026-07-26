#include "state_sensor.hpp"

#include "common/utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

std::optional<std::string> getStateSetName(StateSetId stateSetId)
{
    switch (stateSetId)
    {
        default:
            return std::nullopt;
    }
}

std::unique_ptr<ObjectEnableIntf> createStateSensorObject(
    sdbusplus::bus_t& bus, const std::string& path)
{
    std::unique_ptr<ObjectEnableIntf> objectEnableIntf;
    try
    {
        objectEnableIntf =
            std::make_unique<ObjectEnableIntf>(bus, path.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create Enable interface for state sensor {PATH} error - {ERROR}",
            "PATH", path, "ERROR", e);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    return objectEnableIntf;
}

StateSensor::StateSensor(const pldm_tid_t tid,
                         std::shared_ptr<StateSensorInfo> info, uint8_t offset,
                         const std::string& stateSetName,
                         const std::string& name) :
    tid(tid), offset(offset), info(info), stateSetName(stateSetName), name(name)
{
    if (!info || offset >= info->compositeInfo.size())
    {
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    sensorId = info->pdr.sensor_id;
    path = "/xyz/openbmc_project/state/" + stateSetName + "/" + name;

    auto& bus = pldm::utils::DBusHandler::getBus();
    objectEnableIntf = createStateSensorObject(bus, path);
}

void StateSensor::updateReading(uint8_t sensorOpState, uint8_t presentState)
{
    this->sensorOpState = sensorOpState;
    this->presentState = presentState;
    enabled(sensorOpState == PLDM_SENSOR_ENABLED);
}

void StateSensor::handleErrGetStateSensorReading()
{
    sensorOpState = PLDM_SENSOR_UNAVAILABLE;
    presentState = 0;
    enabled(false);
}

} // namespace platform_mc
} // namespace pldm
