#include "state_sensor.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

/* A State Sensor PDR carries no updateInterval, so the sensor is re-read at
 * the interval a numeric sensor uses when its PDR omits one, in milliseconds.
 */
static constexpr uint64_t defaultStateSensorUpdaterInterval = 999;

StateSensor::StateSensor(pldm_tid_t tid, std::shared_ptr<StateSensorInfo> info,
                         StateSets& stateSets) :
    updateTime(defaultStateSensorUpdaterInterval * 1000), tid(tid),
    info(std::move(info))
{
    for (const auto& componentInfo : this->info->componentInfo)
    {
        const auto stateSetId = componentInfo.first;
        auto stateSet = stateSets.getStateSet(stateSetId);
        if (!stateSet)
        {
            lg2::info(
                "Terminus ID {TID}: Skip state set {STATESETID} of state sensor {SENSORID} - no D-Bus interface.",
                "TID", tid, "STATESETID", stateSetId, "SENSORID",
                this->info->pdr.sensor_id);
        }
        componentStateSets.emplace_back(stateSet);
    }
}

void StateSensor::updatePresentState(uint8_t offset, uint8_t presentState)
{
    if (offset >= componentStateSets.size())
    {
        lg2::error(
            "Terminus ID {TID}: State sensor {SENSORID} has no composite sensor offset {OFFSET}.",
            "TID", tid, "SENSORID", info->pdr.sensor_id, "OFFSET", offset);
        return;
    }

    auto stateSet = componentStateSets[offset];
    if (!stateSet)
    {
        return;
    }

    stateSet->setPresentState(presentState);
}

} // namespace platform_mc
} // namespace pldm
