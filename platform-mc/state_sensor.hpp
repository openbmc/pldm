#pragma once

#include "libpldm/platform.h"
#include "libpldm/pldm.h"

#include "common/types.hpp"
#include "libpldmresponder/event_parser.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Inventory/Source/PLDM/Entity/server.hpp>

namespace pldm
{
namespace platform_mc
{
using namespace pldm::pdr;

using EntityIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Source::PLDM::server::Entity>;

/**
 * @brief ComponentStateSensor
 * This class represents a component sensor of a state sensor
 */
class ComponentStateSensor
{
  public:
    ComponentStateSensor(
        [[maybe_unused]] const pdr::SensorID& sensorId,
        const std::string& sensorName, const std::string& invPath,
        const std::string& terminusName, const pdr::EntityType& entityType,
        const pdr::EntityInstance& entityInstance,
        const pdr::ContainerID& containerID,
        const pdr::SensorOffset& sensorOffset,
        const pdr::StateSetId& stateSetId,
        const pdr::PossibleStates& possibleStates,
        pldm::responder::events::StateSensorHandler& stateSensorHandler);
    ~ComponentStateSensor() = default;

    /** @brief Process the Operational reading of the sensor
     *
     *  @param[in] state - operational state
     *  @return int - PLDM return code
     */
    int processOpState(const pdr::EventState& state);

    /** @brief Process the state reading of the sensor and write to D-Bus
     *
     *  @param[in] state - present state
     *  @return int - PLDM return code
     */
    int processSensorState(const pdr::EventState& state);

    /** @brief Sensor name */
    std::string sensorName;

  private:
    /** @brief Sensor operational status */
    bool opState = false;
    /** @brief Inventory.Source.PLDM.Entity D-Bus interface */
    std::unique_ptr<EntityIntf> entityIntf = nullptr;
    /** @brief Sensor possible states from PDR as a vector of bytes */
    pdr::PossibleStates possibleStates;
    /** @brief The handler to hold JSON config info of the sensor */
    pldm::responder::events::StateSensorHandler& stateSensorHandler;
    /** @brief The entry of the sensor to be mapped with JSON configs */
    pldm::responder::events::StateSensorEntry entry;
};

/**
 * @brief StateSensor
 *
 * This class represent a state sensor (can be a composite sensor),
 * manages component sensors, handles readings and forwards results
 * to the corresponding component sensor
 */
class StateSensor
{
  public:
    StateSensor(
        const pldm_tid_t tid, const std::string& terminusName,
        const std::shared_ptr<pdr::PDR> pdr,
        const std::vector<std::string>& sensorNames,
        const std::string& associationPath,
        pldm::responder::events::StateSensorHandler& stateSensorHandler);

    ~StateSensor() {};

    /** @brief Forward the operational reading to the component sensor
     *
     *  @param[in] state - operational state
     *  @param[in] sensorOffset - component sensor offset
     *  @return int - PLDM return code
     */
    inline int processOpState(const pdr::EventState& state,
                              const pdr::SensorOffset& sensorOffset)
    {
        if (sensorOffset < componentSensors.size() &&
            componentSensors[sensorOffset])
        {
            return componentSensors[sensorOffset]->processOpState(state);
        }
        return PLDM_ERROR;
    }
    /** @brief Forward the state reading to the component sensor
     *
     *  @param[in] state - present state
     *  @param[in] sensorOffset - component sensor offset
     *  @return int - PLDM return code
     */
    inline int processSensorState(const pdr::EventState& state,
                                  const pdr::SensorOffset& sensorOffset)
    {
        if (sensorOffset < componentSensors.size() &&
            componentSensors[sensorOffset])
        {
            return componentSensors[sensorOffset]->processSensorState(state);
        }
        return PLDM_ERROR;
    }
    /** @brief Process the state sensor reading from GetStateSensorReading
     * response
     *
     *  @param[in] stateField - state field struct to hold readings
     *  @param[in] comp_sensor_count - number of component sensors
     *  @return int - PLDM return code
     */
    int processStateSensorReadings(
        const std::array<get_sensor_state_field,
                         PLDM_STATE_SENSOR_MAX_COMPOSITE_SENSOR_COUNT>&
            stateField,
        const pdr::CompositeCount& comp_sensor_count);

    /** @brief Terminus ID which the sensor belongs to */
    pldm_tid_t tid;
    /** @brief Sensor ID */
    pdr::SensorID sensorId;
    /** @brief Sensor composite count */
    pdr::CompositeCount compositeCount;

  private:
    /** @brief List of pointers to component sensors*/
    std::vector<std::unique_ptr<ComponentStateSensor>> componentSensors;
};
} // namespace platform_mc
} // namespace pldm
