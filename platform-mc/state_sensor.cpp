#include "state_sensor.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

StateSensor::StateSensor(pldm_tid_t tid, std::shared_ptr<StateSensorInfo> info,
                         std::shared_ptr<StateSets> stateSets) :
    tid(tid), info(std::move(info)), stateSets(std::move(stateSets))
{
    for (const auto& compositeInfo : this->info->compositeInfo)
    {
        const auto stateSetId = compositeInfo.first;
        auto stateSet = this->stateSets->getStateSet(stateSetId);
        if (!stateSet)
        {
            lg2::info(
                "Terminus ID {TID}: Skip state set {STATESETID} of state sensor {SENSORID} - no D-Bus interface.",
                "TID", tid, "STATESETID", stateSetId, "SENSORID",
                this->info->pdr.sensor_id);
        }
        compositeStateSets.emplace_back(stateSet);
    }
}

void StateSensor::updatePresentState(uint8_t offset, uint8_t presentState)
{
    if (offset >= compositeStateSets.size())
    {
        lg2::error(
            "Terminus ID {TID}: State sensor {SENSORID} has no composite sensor offset {OFFSET}.",
            "TID", tid, "SENSORID", info->pdr.sensor_id, "OFFSET", offset);
        return;
    }

    auto stateSet = compositeStateSets[offset];
    if (!stateSet)
    {
        return;
    }

    stateSet->setPresentState(presentState);
}

} // namespace platform_mc
} // namespace pldm
