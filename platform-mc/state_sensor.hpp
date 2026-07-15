#pragma once

#include "common/types.hpp"
#include "common/utils.hpp"

#include <libpldm/platform.h>
#include <libpldm/pldm.h>

#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Source/PLDM/Entity/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace pldm
{
namespace platform_mc
{

using namespace pldm::pdr;

using OperationalStatusIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::State::
                                    Decorator::server::OperationalStatus>;
using AssociationDefinitionsInft = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;
using EntityIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Source::PLDM::server::Entity>;

/** @struct StateSensorInfo
 *
 *  The parsed State Sensor PDR: the fixed portion decoded by
 *  decode_pldm_platform_state_sensor_pdr() plus, for each composite sensor
 *  offset, the state set ID and the possible state values decoded from the
 *  possible_states[] region.
 */
struct StateSensorInfo
{
    pldm_platform_state_sensor_pdr pdr;

    /** @brief State set ID and supported state values of each composite
     *         sensor offset
     */
    std::vector<std::pair<StateSetId, std::set<uint8_t>>> compositeInfo;
};

/** @struct StateSensorReading
 *
 *  Raw values of one component sensor from GetStateSensorReadings, kept as
 *  wire values until a state set implementation converts them to D-Bus
 *  properties.
 */
struct StateSensorReading
{
    uint8_t opState;
    uint8_t presentState;
    uint8_t previousState;
};

/**
 * @brief StateSensor
 *
 * This class handles one component sensor of a PLDM state sensor. It owns the
 * D-Bus object at /xyz/openbmc_project/sensors/<stateSetName>/<label> and
 * stores the raw state of its composite sensor offset updated by the sensor
 * manager.
 *
 * There is no D-Bus object for the composite state sensor itself: a state
 * sensor with N component sensors creates N StateSensor objects, one per
 * offset. A component sensor whose state set has no snake_case name (an OEM
 * or otherwise unmapped state set) gets no object.
 */
class StateSensor
{
  public:
    /** @brief Constructor
     *
     *  @param[in] tid - terminus ID of the sensor
     *  @param[in] sensorDisabled - initial functional state
     *  @param[in] info - parsed state sensor PDR
     *  @param[in] offset - composite sensor offset this object represents
     *  @param[in] stateSetName - snake_case state set name, the sensors
     *             namespace path element
     *  @param[in] label - object leaf name
     *  @param[in] associationPath - inventory path of the terminus
     */
    StateSensor(const pldm_tid_t tid, const bool sensorDisabled,
                std::shared_ptr<StateSensorInfo> info, uint8_t offset,
                const std::string& stateSetName, const std::string& label,
                const std::string& associationPath);

    ~StateSensor() {};

    /** @brief The function called by Sensor Manager to set sensor to
     *  error status.
     */
    void handleErrGetStateSensorReadings();

    /** @brief Store the raw state of this component sensor and refresh the
     *  OperationalStatus.
     *
     *  @param[in] opState - sensorOperationalState wire value
     *  @param[in] presentState - presentState wire value
     *  @param[in] previousState - previousState wire value
     */
    void updateReading(uint8_t opState, uint8_t presentState,
                       uint8_t previousState);

    /** @brief Get the stored raw state of this component sensor
     *
     *  @return the stored state, or std::nullopt before the first update
     */
    std::optional<StateSensorReading> getReading() const
    {
        return reading;
    }

    /** @brief Get the OperationalStatus functional value */
    bool functional() const
    {
        return operationalStatusIntf && operationalStatusIntf->functional();
    }

    /** @brief Whether the initial GetStateSensorReadings succeeded. */
    bool initialized = false;

    /** @brief Terminus ID which the sensor belongs to */
    pldm_tid_t tid;

    /** @brief Sensor ID */
    SensorID sensorId;

    /** @brief Composite sensor offset this object represents */
    uint8_t offset;

    /** @brief The parsed PDR of the sensor */
    std::shared_ptr<StateSensorInfo> info;

    /** @brief State set name, the sensors namespace path element */
    std::string stateSetName;

    /** @brief Object leaf name */
    std::string label;

    /** @brief Sensor D-Bus object path */
    std::string path;

    /** @brief Timestamp (CLOCK_MONOTONIC us) of the last successful read */
    uint64_t timeStamp = 0;

    /** @brief Minimum interval (us) between reads on the polling task */
    uint64_t updateTime;

  private:
    /** @brief Refresh OperationalStatus from this component's own
     *  sensorOperationalState.
     */
    void updateFunctional();

    /** @brief Create the companion inventory object with the PLDM entity
     *  information of the sensor
     *
     *  @param[in] associationPath - inventory path of the terminus
     *  @param[in] label - object leaf name
     *  @return true on success
     */
    bool createInventoryPath(const std::string& associationPath,
                             const std::string& label);

    /** @brief Raw state of this component sensor */
    std::optional<StateSensorReading> reading;

    std::unique_ptr<AssociationDefinitionsInft> associationDefinitionsIntf =
        nullptr;
    std::unique_ptr<OperationalStatusIntf> operationalStatusIntf = nullptr;
    std::unique_ptr<EntityIntf> entityIntf = nullptr;
};
} // namespace platform_mc
} // namespace pldm
