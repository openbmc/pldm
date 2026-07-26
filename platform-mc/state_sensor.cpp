#include "state_sensor.hpp"

#include "common/utils.hpp"

#include <libpldm/state_set.h>

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
        case PLDM_STATE_SET_HEALTH_STATE:
            return "health";
        default:
            return std::nullopt;
    }
}

bool healthStateToFunctional(uint8_t presentState)
{
    return presentState == PLDM_STATE_SET_HEALTH_STATE_NORMAL;
}

HealthStateSetIntf::HealthStateSetIntf(sdbusplus::bus_t& bus,
                                       const std::string& path,
                                       const std::string& inventoryPath)
{
    try
    {
        operationalStatusIntf =
            std::make_unique<OperationalStatusIntf>(bus, path.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create OperationalStatus interface for state sensor {PATH} error - {ERROR}",
            "PATH", path, "ERROR", e);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    if (inventoryPath.empty())
    {
        return;
    }

    try
    {
        associationDefinitionsIntf =
            std::make_unique<AssociationDefinitionsIntf>(bus, path.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create association interface for state sensor {PATH} error - {ERROR}",
            "PATH", path, "ERROR", e);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    /* The health reported here is the health of the monitored inventory item,
     * not of this object, so the object is possessed by that item. */
    associationDefinitionsIntf->associations(
        {{"possessed_by", "possessing", inventoryPath}});
}

void HealthStateSetIntf::updateState(uint8_t presentState)
{
    operationalStatusIntf->functional(healthStateToFunctional(presentState));
}

void HealthStateSetIntf::clearState()
{
    operationalStatusIntf->functional(false);
}

std::unique_ptr<StateSetIntf> createStateSetIntf(
    StateSetId stateSetId, sdbusplus::bus_t& bus, const std::string& path,
    const std::string& inventoryPath)
{
    switch (stateSetId)
    {
        case PLDM_STATE_SET_HEALTH_STATE:
            return std::make_unique<HealthStateSetIntf>(bus, path,
                                                        inventoryPath);
        default:
            return nullptr;
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

StateSensor::StateSensor(
    const pldm_tid_t tid, std::shared_ptr<StateSensorInfo> info, uint8_t offset,
    const std::string& stateSetName, const std::string& name,
    const std::string& inventoryPath) :
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

    stateSetIntf = createStateSetIntf(info->compositeInfo[offset].first, bus,
                                      path, inventoryPath);
    if (!stateSetIntf)
    {
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }
}

void StateSensor::updateReading(uint8_t sensorOpState, uint8_t presentState)
{
    this->sensorOpState = sensorOpState;
    this->presentState = presentState;
    enabled(sensorOpState == PLDM_SENSOR_ENABLED);

    if (sensorOpState == PLDM_SENSOR_ENABLED)
    {
        stateSetIntf->updateState(presentState);
    }
    else
    {
        stateSetIntf->clearState();
    }
}

void StateSensor::handleErrGetStateSensorReading()
{
    sensorOpState = PLDM_SENSOR_UNAVAILABLE;
    presentState = 0;
    enabled(false);
    stateSetIntf->clearState();
}

} // namespace platform_mc
} // namespace pldm
