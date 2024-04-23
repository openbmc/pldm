#include "state_sensor.hpp"

#include "libpldm/platform.h"

#include "common/utils.hpp"
#include "requester/handler.hpp"

#include <phosphor-logging/lg2.hpp>

#include <limits>

namespace pldm
{
namespace platform_mc
{

ComponentStateSensor::ComponentStateSensor(
    [[maybe_unused]] const pdr::SensorID& sensorId,
    const std::string& sensorName, const std::string& invPath,
    const pdr::EntityType& entityType,
    const pdr::EntityInstance& entityInstance,
    const pdr::ContainerID& containerID, const pdr::SensorOffset& sensorOffset,
    const pdr::StateSetId& stateSetId,
    const pdr::PossibleStates& possibleStates,
    pldm::responder::events::StateSensorHandler& stateSensorHandler) :
    sensorName(sensorName), possibleStates(possibleStates),
    stateSensorHandler(stateSensorHandler),
    entry({containerID, entityType, entityInstance, sensorOffset, stateSetId,
           false})
{
    auto& bus = pldm::utils::DBusHandler::getBus();

    try
    {
        entityIntf = std::make_unique<EntityIntf>(bus, invPath.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create Entity interface for path {PATH} error - {ERROR}",
            "PATH", invPath, "ERROR", e);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    entityIntf->entityType(entityType);
    entityIntf->entityInstanceNumber(entityInstance);
    entityIntf->containerID(containerID);

    lg2::info("Created Component State Sensor {NAME}", "NAME", sensorName);
}

int ComponentStateSensor::processOpState(const pdr::EventState& state)
{
    if (state == PLDM_SENSOR_ENABLED)
    {
        opState = true;
    }
    else
    {
        opState = false;
    }
    return PLDM_SUCCESS;
}

int ComponentStateSensor::processSensorState(const pdr::EventState& state)
{
    if (!opState)
    {
        return PLDM_ERROR;
    }

    if (!possibleStates.contains(state))
    {
        lg2::error("Invalid state sensor state {STATE}", "STATE", state);
        return PLDM_ERROR_INVALID_DATA;
    }

    auto rc = stateSensorHandler.eventAction(entry, state);
    if (rc)
    {
        error("Failed to fetch and update D-bus property, response code '{RC}'",
              "RC", rc);
    }
    return rc;
}

StateSensor::StateSensor(
    const pldm_tid_t tid, const std::shared_ptr<pdr::PDR> pdr,
    const std::vector<std::string>& sensorNames,
    const std::string& associationPath,
    pldm::responder::events::StateSensorHandler& stateSensorHandler) : tid(tid)
{
    auto pdrData = new (pdr->data())(pldm_state_sensor_pdr);

    pdr::EntityType entityType = std::numeric_limits<uint16_t>::quiet_NaN();
    pdr::EntityInstance entityInstance =
        std::numeric_limits<uint16_t>::quiet_NaN();
    pdr::ContainerID containerID = std::numeric_limits<uint16_t>::quiet_NaN();
    pdr::CompositeStates sensors;

    sensorId = pdrData->sensor_id;
    entityType = pdrData->entity_type;
    entityInstance = pdrData->entity_instance;
    containerID = pdrData->container_id;
    compositeCount = pdrData->composite_sensor_count;

    auto statesPtr = pdrData->possible_states;
    pdr::CompositeCount count = compositeCount;

    while (count--)
    {
        auto state = new (statesPtr)(state_sensor_possible_states);

        pdr::PossibleStates possibleStates{};
        pdr::StateSetId stateSetId = state->state_set_id;
        uint8_t possibleStatesPos{};
        auto updateStates = [&possibleStates,
                             &possibleStatesPos](const bitfield8_t& val) {
            for (const auto& i :
                 std::views::iota(0, static_cast<int>(CHAR_BIT)))
            {
                if (val.byte & (1 << i))
                {
                    possibleStates.insert(possibleStatesPos * CHAR_BIT + i);
                }
            }
            possibleStatesPos++;
        };
        std::for_each(&state->states[0],
                      &state->states[state->possible_states_size],
                      updateStates);

        sensors.emplace_back(
            std::make_tuple(std::move(stateSetId), std::move(possibleStates)));

        if (count)
        {
            statesPtr += sizeof(state_sensor_possible_states) +
                         state->possible_states_size - 1;
        }
    }

    if (sensors.size() != sensorNames.size())
    {
        throw std::runtime_error("Invalid sensorNames size");
    }

    std::string invPath;

    // Create component sensor instances
    pdr::SensorOffset sensorOffset = 0;
    for (const auto& name : sensorNames)
    {
        invPath = associationPath + "/" + name;

        const auto& stateSetId = std::get<0>(sensors[sensorOffset]);
        const auto& possibleStates = std::get<1>(sensors[sensorOffset]);

        std::unique_ptr<ComponentStateSensor> componentSensor;
        componentSensor = std::make_unique<ComponentStateSensor>(
            sensorId, name, invPath, entityType, entityInstance, containerID,
            sensorOffset, stateSetId, possibleStates, stateSensorHandler);

        componentSensors.emplace_back(std::move(componentSensor));
        sensorOffset++;
    }
}

int StateSensor::processStateSensorReadings(
    const std::array<get_sensor_state_field,
                     PLDM_STATE_SENSOR_MAX_COMPOSITE_SENSOR_COUNT>& stateField,
    const pdr::CompositeCount& comp_sensor_count)
{
    pdr::EventState operationalState, presentState;

    for (const auto& sensorOffset :
         std::views::iota(0, static_cast<int>(comp_sensor_count)))
    {
        operationalState = stateField[sensorOffset].sensor_op_state;
        presentState = stateField[sensorOffset].present_state;

        if (sensorOffset >= compositeCount)
        {
            lg2::error(
                "Invalid state sensor offset in StateSensorReadings for "
                "terminus ID {TID}, sensor ID {ID}, sensor offset {OFFSET}",
                "TID", tid, "ID", sensorId, "OFFSET", sensorOffset);
            return PLDM_ERROR_INVALID_DATA;
        }

        auto rc = processOpState(operationalState, sensorOffset);
        if (rc)
        {
            lg2::error(
                "Failed to proccess StateSensorReadings operational state for "
                "terminus ID {TID}, sensor ID {ID}, sensor offset {OFFSET}, error {RC}",
                "TID", tid, "ID", sensorId, "OFFSET", sensorOffset, "RC", rc);
        }
        rc = processSensorState(presentState, sensorOffset);
        if (rc)
        {
            lg2::error(
                "Failed to proccess StateSensorReadings present state for "
                "terminus ID {TID}, sensor ID {ID}, sensor offset {OFFSET}, error {RC}",
                "TID", tid, "ID", sensorId, "OFFSET", sensorOffset, "RC", rc);
        }
    }
    return PLDM_SUCCESS;
}

} // namespace platform_mc
} // namespace pldm
