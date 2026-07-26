#pragma once

#include "common/types.hpp"

#include <libpldm/base.h>
#include <libpldm/platform.h>

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Object/Enable/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

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
using OperationalStatusIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::State::
                                    Decorator::server::OperationalStatus>;
using AssociationDefinitionsIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

/** @struct StateSensorInfo
 *
 *  The parsed State Sensor PDR: the fixed fields the framework needs plus,
 *  for each composite sensor offset, the state set ID and the possible state
 *  values parsed from the possible_states[] region. This is the parse-layer
 *  representation the state sensor object creation consumes.
 */
struct StateSensorInfo
{
    /** @brief Fixed fields taken from the State Sensor PDR header. Held in a
     *  local struct rather than a libpldm PDR type so the framework does not
     *  depend on a specific decode routine.
     */
    struct
    {
        uint16_t sensor_id;
        uint16_t entity_type;
        uint16_t entity_instance_number;
        uint16_t container_id;
        uint8_t composite_sensor_count;
    } pdr;

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

/** @brief The D-Bus interface which publishes the state values of one state
 *         set.
 *
 *  A state set is published through the interface modeled after its Redfish
 *  analog, so each state set brings its own implementation. A component state
 *  sensor owns one of them: the one of the state set its offset reports.
 */
class StateSetIntf
{
  public:
    StateSetIntf() = default;
    virtual ~StateSetIntf() = default;

    /** @brief Publish the state the component sensor has read.
     *
     *  @param[in] presentState - present state of the component sensor
     */
    virtual void updateState(uint8_t presentState) = 0;

    /** @brief Publish that the component sensor has no state. */
    virtual void clearState() = 0;
};

/** @brief Map a Health state set value onto the OperationalStatus Functional
 *         property.
 *
 *  Functional is a boolean while the DSP0249 Health state set holds ten
 *  values, so only Normal is functional and every other value, including the
 *  non-critical ones, collapses onto false.
 *
 *  @param[in] presentState - present state of the component sensor
 *  @return the Functional property value
 */
bool healthStateToFunctional(uint8_t presentState);

/** @brief The Health state set, published through
 *         xyz.openbmc_project.State.Decorator.OperationalStatus.
 *
 *  The Health state set reports the health of the entity the state sensor
 *  monitors, which is what the Functional property carries.
 */
class HealthStateSetIntf : public StateSetIntf
{
  public:
    /** @brief Constructor
     *
     *  @param[in] bus - D-Bus connection
     *  @param[in] path - component state sensor object path
     *  @param[in] inventoryPath - object path of the inventory item whose
     *             health is reported, empty when the terminus has no
     *             inventory object
     */
    HealthStateSetIntf(sdbusplus::bus_t& bus, const std::string& path,
                       const std::string& inventoryPath);

    void updateState(uint8_t presentState) override;

    void clearState() override;

  private:
    std::unique_ptr<OperationalStatusIntf> operationalStatusIntf = nullptr;
    std::unique_ptr<AssociationDefinitionsIntf> associationDefinitionsIntf =
        nullptr;
};

/** @brief Create the interface publishing the state values of a state set.
 *
 *  @param[in] stateSetId - PLDM state set ID of the component sensor
 *  @param[in] bus - D-Bus connection
 *  @param[in] path - component state sensor object path
 *  @param[in] inventoryPath - object path of the inventory item the state
 *             sensor monitors
 *  @return the created interface, or nullptr when the state set is unmapped
 */
std::unique_ptr<StateSetIntf> createStateSetIntf(
    StateSetId stateSetId, sdbusplus::bus_t& bus, const std::string& path,
    const std::string& inventoryPath);

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
 * The object lives at /xyz/openbmc_project/state/<stateSetName>/<name>. It
 * carries xyz.openbmc_project.Object.Enable, which reports the state sensor's
 * own operational state, plus the interface publishing the state values of the
 * offset's state set.
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
     *  @param[in] inventoryPath - object path of the inventory item the state
     *             sensor monitors
     */
    StateSensor(const pldm_tid_t tid, std::shared_ptr<StateSensorInfo> info,
                uint8_t offset, const std::string& stateSetName,
                const std::string& name, const std::string& inventoryPath);

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
     *  state is enabled, so that is what the object is enabled from and what
     *  gates publishing the state value.
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

    /** @brief Interface publishing the state values of the offset's state set
     */
    std::unique_ptr<StateSetIntf> stateSetIntf = nullptr;
};

} // namespace platform_mc
} // namespace pldm
