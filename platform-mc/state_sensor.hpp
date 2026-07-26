#pragma once

#include "common/types.hpp"

#include <libpldm/base.h>
#include <libpldm/platform.h>

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Object/Enable/server.hpp>

#include <cstdint>
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

using ObjectEnableIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Object::server::Enable>;

/** @struct StateSensorInfo
 *
 *  The parsed State Sensor PDR: the fixed fields the framework needs plus,
 *  for each composite sensor offset, the state set ID and the possible state
 *  values parsed from the possible_states[] region. This is the parse-layer
 *  representation the state sensor object creation consumes.
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
    std::vector<std::pair<StateSetId, std::set<uint8_t>>> compositeInfo;
};

/** @brief Get the name of a state set, used as the state namespace element of
 *         a component state sensor object path.
 *
 *  A state set is mapped here only once the D-Bus interface publishing its
 *  state values is defined, so an unmapped state set has no name and its
 *  component sensor gets no object.
 *
 *  @param[in] stateSetId - PLDM state set ID of the component sensor
 *  @return the state set name, or std::nullopt when the state set is unmapped
 */
std::optional<std::string> getStateSetName(StateSetId stateSetId);

/** @brief Create the D-Bus object of a component state sensor.
 *
 *  Every component state sensor object is created through this function, so
 *  they all carry xyz.openbmc_project.Object.Enable. Creating that interface
 *  is what brings the object onto the bus. The object starts disabled: a
 *  component sensor has no state until it has been read.
 *
 *  @param[in] bus  - D-Bus connection
 *  @param[in] path - component state sensor object path
 *  @return the created Object.Enable interface
 */
std::unique_ptr<ObjectEnableIntf> createStateSensorObject(
    sdbusplus::bus_t& bus, const std::string& path);

/**
 * @brief StateSensor
 *
 * One component sensor of a PLDM state sensor, and the D-Bus object which
 * publishes it. A composite state sensor with N component sensors has N
 * StateSensor instances, one per offset; the composite sensor itself has no
 * D-Bus object.
 *
 * The object lives at /xyz/openbmc_project/state/<stateSetName>/<name> and
 * carries xyz.openbmc_project.Object.Enable. The interface publishing the
 * state value is added per state set as those state sets are mapped.
 */
class StateSensor
{
  public:
    /** @brief Constructor
     *
     *  @param[in] tid - terminus ID the state sensor belongs to
     *  @param[in] info - parsed State Sensor PDR of the composite sensor
     *  @param[in] offset - composite sensor offset this object represents
     *  @param[in] stateSetName - name of the offset's state set, the state
     *             namespace element of the object path
     *  @param[in] name - object leaf name
     */
    StateSensor(const pldm_tid_t tid, std::shared_ptr<StateSensorInfo> info,
                uint8_t offset, const std::string& stateSetName,
                const std::string& name);

    ~StateSensor() = default;

    /** @brief Get the Enabled property */
    bool enabled() const
    {
        return objectEnableIntf->enabled();
    }

    /** @brief Set the Enabled property
     *
     *  @param[in] value - the new Enabled value
     */
    void enabled(bool value)
    {
        objectEnableIntf->enabled(value);
    }

    /** @brief Take the state of this component sensor from a
     *         GetStateSensorReadings response.
     *
     *  The component sensor only has a state while its own sensor operational
     *  state is enabled, so that is what the object is enabled from.
     *
     *  @param[in] sensorOpState - operational state of the component sensor
     *  @param[in] presentState - present state of the component sensor
     */
    void updateReading(uint8_t sensorOpState, uint8_t presentState);

    /** @brief Handle a GetStateSensorReadings which did not yield a state for
     *         this component sensor. The state is no longer known, so the
     *         object is disabled.
     */
    void handleErrGetStateSensorReading();

    /** @brief Terminus ID which the state sensor belongs to */
    pldm_tid_t tid;

    /** @brief Sensor ID of the composite state sensor */
    SensorID sensorId;

    /** @brief Composite sensor offset this object represents */
    uint8_t offset;

    /** @brief The parsed PDR of the composite state sensor */
    std::shared_ptr<StateSensorInfo> info;

    /** @brief Name of the offset's state set */
    std::string stateSetName;

    /** @brief Object leaf name */
    std::string name;

    /** @brief Component state sensor D-Bus object path */
    std::string path;

    /** @brief Operational state of the component sensor, from the last
     *         GetStateSensorReadings
     */
    uint8_t sensorOpState = PLDM_SENSOR_UNAVAILABLE;

    /** @brief Present state of the component sensor, from the last
     *         GetStateSensorReadings. 0 while the component sensor has no
     *         state: every PLDM state set reserves state value 0.
     */
    uint8_t presentState = 0;

  private:
    std::unique_ptr<ObjectEnableIntf> objectEnableIntf = nullptr;
};

} // namespace platform_mc
} // namespace pldm
