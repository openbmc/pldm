#pragma once

#include "common/types.hpp"
#include "state_set.hpp"

#include <libpldm/platform.h>

#include <cstdint>
#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace pldm
{
namespace platform_mc
{

using namespace pldm::pdr;

/** @struct StateSensorInfo
 *
 *  The parsed State Sensor PDR: the fixed fields the framework needs plus,
 *  for each composite sensor offset, the state set ID and the possible state
 *  values parsed from the possible_states[] region.
 */
struct StateSensorInfo
{
    /** @brief Fixed portion of the State Sensor PDR, up to and including
     *         compositeSensorCount
     */
    pldm_platform_state_sensor_pdr pdr;

    /** @brief State set ID and supported state values of each composite
     *         sensor offset
     */
    std::vector<std::pair<StateSetId, PossibleStates>> compositeInfo;
};

/**
 * @brief StateSensor
 *
 * One state sensor of a terminus. A state sensor has no D-Bus object of its
 * own: each of its component sensors reports the state of the entity which
 * the State Sensor PDR identifies, so the state is published through the
 * state set interface of that entity's D-Bus object. A component sensor whose
 * state set has no D-Bus interface publishes nothing.
 */
class StateSensor
{
  public:
    StateSensor() = delete;
    StateSensor(const StateSensor&) = delete;
    StateSensor& operator=(const StateSensor&) = delete;
    StateSensor(StateSensor&&) = delete;
    StateSensor& operator=(StateSensor&&) = delete;
    ~StateSensor() = default;

    /** @brief Constructor
     *
     *  @param[in] tid - terminus ID of the sensor
     *  @param[in] info - the parsed State Sensor PDR
     *  @param[in] stateSets - the state set interfaces of the D-Bus object of
     *                         the entity which the PDR identifies
     */
    StateSensor(pldm_tid_t tid, std::shared_ptr<StateSensorInfo> info,
                std::shared_ptr<StateSets> stateSets);

    /** @brief The getter to return the terminus ID of the sensor */
    pldm_tid_t getTid() const
    {
        return tid;
    }

    /** @brief The getter to return the sensor ID */
    SensorID getSensorId() const
    {
        return info->pdr.sensor_id;
    }

    /** @brief The getter to return the number of component sensors */
    uint8_t getCompositeSensorCount() const
    {
        return info->pdr.composite_sensor_count;
    }

    /** @brief Whether an initialization agent has to set the operational
     *         state of the component sensors, `sensorInit` of `Table 81 -
     *         State Sensor PDR` of DSP0248 v1.3.0. A sensor which reports
     *         noInit is operational as the terminus starts up and takes no
     *         SetStateSensorEnables.
     */
    bool requiresInit() const
    {
        return info->pdr.sensor_init != PLDM_NO_INIT;
    }

    /** @brief Publish the state which one component sensor reports on the
     *         state set interface of the entity
     *
     *  @param[in] offset - composite sensor offset of the component sensor
     *  @param[in] presentState - the presentState of GetStateSensorReadings
     */
    void updatePresentState(uint8_t offset, uint8_t presentState);

    /** @brief Whether the component sensors of the sensor have been enabled
     *         by SetStateSensorEnables
     */
    bool enabled = false;

    /** @brief Whether the terminus answered SetStateSensorEnables with an
     *         error completion code. The answer is definitive, so the command
     *         is not sent for the sensor again.
     */
    bool enableRejected = false;

    /** @brief Timestamp (CLOCK_MONOTONIC us) of the last successful read */
    uint64_t timeStamp = 0;

    /** @brief Minimum interval (us) between two reads of the sensor */
    uint64_t updateTime;

  private:
    /** @brief Terminus ID which the sensor belongs to */
    pldm_tid_t tid;

    /** @brief The parsed State Sensor PDR of the sensor */
    std::shared_ptr<StateSensorInfo> info;

    /** @brief The state set interfaces of the D-Bus object of the entity */
    std::shared_ptr<StateSets> stateSets;

    /** @brief The state set interface of each composite sensor offset,
     *         nullptr when the state set has no D-Bus interface
     */
    std::vector<StateSetBase*> compositeStateSets;
};

} // namespace platform_mc
} // namespace pldm
