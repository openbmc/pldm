#include "state_sensor.hpp"

#include "common/utils.hpp"

#include <libpldm/platform.h>

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

/* State Sensor PDRs carry no updateInterval, so re-poll at the same default
 * cadence (milliseconds) numeric sensors use when their PDR omits one. */
static constexpr uint64_t stateSensorUpdaterIntervalMs = 999;

StateSensor::StateSensor(const pldm_tid_t tid, const bool sensorDisabled,
                         std::shared_ptr<StateSensorInfo> info, uint8_t offset,
                         const std::string& stateSetName,
                         const std::string& label,
                         const std::string& associationPath) :
    tid(tid), offset(offset), info(info), stateSetName(stateSetName),
    label(label), updateTime(stateSensorUpdaterIntervalMs * 1000)
{
    if (!info || offset >= info->compositeInfo.size())
    {
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    sensorId = info->pdr.sensor_id;
    path = "/xyz/openbmc_project/sensors/" + stateSetName + "/" + label;

    auto& bus = pldm::utils::DBusHandler::getBus();
    try
    {
        associationDefinitionsIntf =
            std::make_unique<AssociationDefinitionsInft>(bus, path.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create association interface for state sensor {PATH} error - {ERROR}",
            "PATH", path, "ERROR", e);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }
    associationDefinitionsIntf->associations(
        {{"chassis", "all_states", associationPath}});

    if (!createInventoryPath(associationPath, label))
    {
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    try
    {
        operationalStatusIntf =
            std::make_unique<OperationalStatusIntf>(bus, path.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create Operation status interface for state sensor {PATH} error - {ERROR}",
            "PATH", path, "ERROR", e);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }
    operationalStatusIntf->functional(!sensorDisabled);
}

bool StateSensor::createInventoryPath(const std::string& associationPath,
                                      const std::string& label)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::string invPath = associationPath + "/" + label;
    try
    {
        entityIntf = std::make_unique<EntityIntf>(bus, invPath.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create Entity interface for state sensor {PATH} error - {ERROR}",
            "PATH", invPath, "ERROR", e);
        return false;
    }
    entityIntf->entityType(info->pdr.entity_type);
    entityIntf->entityInstanceNumber(info->pdr.entity_instance_number);
    entityIntf->containerID(info->pdr.container_id);

    return true;
}

void StateSensor::updateReading(uint8_t opState, uint8_t presentState,
                                uint8_t previousState)
{
    reading = StateSensorReading{opState, presentState, previousState};
    updateFunctional();
}

void StateSensor::updateFunctional()
{
    if (operationalStatusIntf)
    {
        operationalStatusIntf->functional(
            reading && reading->opState == PLDM_SENSOR_ENABLED);
    }
}

void StateSensor::handleErrGetStateSensorReadings()
{
    reading = std::nullopt;
    if (operationalStatusIntf)
    {
        operationalStatusIntf->functional(false);
    }
}

} // namespace platform_mc
} // namespace pldm
