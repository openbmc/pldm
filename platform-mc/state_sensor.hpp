#pragma once

#include "common/types.hpp"
#include "common/utils.hpp"

#include <libpldm/platform.h>
#include <libpldm/pldm.h>
#include <libpldm/states.h>

#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Source/PLDM/Entity/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Availability/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <map>
#include <string>
#include <vector>

namespace pldm
{
namespace platform_mc
{

constexpr const char* STATE_SENSOR_VALUE_INTF =
    "xyz.openbmc_project.State.Sensor";

using OperationalStatusIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::State::
                                    Decorator::server::OperationalStatus>;
using AvailabilityIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::State::Decorator::server::Availability>;
using AssociationDefinitionsIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;
using EntityIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Source::PLDM::server::Entity>;

/**
 * @brief StateSensor
 *
 * This class handles state sensor reading updated by sensor manager and export
 * status to D-Bus interface according to DSP0248 specification.
 */
class StateSensor
{
  public:
    StateSensor(const pldm_tid_t tid, const bool sensorDisabled,
                std::shared_ptr<pldm_state_sensor_pdr> pdr,
                const std::string& sensorName,
                const std::string& associationPath);

    ~StateSensor() = default;

    /** @brief The function called by Sensor Manager to set sensor to
     * error status.
     */
    void handleErrGetSensorReading();

    /** @brief Updating the sensor status to D-Bus interface
     *
     * @param[in] available - sensor availability status
     * @param[in] functional - sensor functional status
     * @param[in] sensorStates - vector of current state values for composite
     * sensors
     */
    void updateReading(bool available, bool functional,
                       const std::vector<uint8_t>& sensorStates = {});

    /** @brief Get current state for a specific sensor offset
     *
     * @param[in] sensorOffset - offset within composite sensor
     * @return uint8_t - current state value
     */
    uint8_t getCurrentState(uint8_t sensorOffset) const;

    /** @brief Get previous state for a specific sensor offset
     *
     * @param[in] sensorOffset - offset within composite sensor
     * @return uint8_t - previous state value
     */
    uint8_t getPreviousState(uint8_t sensorOffset) const;

    /** @brief Set the sensor inventory path association
     *
     * @param[in] inventoryPath - inventory path of the entity
     */
    inline void setInventoryPath(const std::string& inventoryPath)
    {
        if (associationDefinitionsIntf)
        {
            associationDefinitionsIntf->associations(
                {{"chassis", "all_sensors", inventoryPath}});
        }
    }

    /** @brief Get the number of composite sensors
     *
     * @return uint8_t - number of sensors in the composite
     */
    uint8_t getCompositeSensorCount() const
    {
        return compositeSensorCount;
    }

    /** @brief Get sensor ID
     *
     * @return uint16_t - sensor identifier
     */
    uint16_t getSensorId() const
    {
        return sensorId;
    }

    /** @brief Get terminus ID which the sensor belongs to */
    pldm_tid_t getTid() const
    {
        return tid;
    }

    /** @brief Get sensor name */
    const std::string& getSensorName() const
    {
        return sensorName;
    }

    /** @brief Check if a specific state is supported for a sensor offset
     *
     * @param[in] sensorOffset - offset within composite sensor
     * @param[in] state - state value to check
     * @return bool - true if state is supported
     */
    bool isStateSupported(uint8_t sensorOffset, uint8_t state) const;

  private:
    /** @brief Create the sensor inventory path
     *
     * @param[in] associationPath - sensor association path
     * @param[in] sensorName - sensor name
     * @param[in] entityType - sensor PDR entity type
     * @param[in] entityInstanceNum - sensor PDR entity instance number
     * @param[in] containerId - sensor PDR entity container ID
     *
     * @return bool - true on success, false on failure
     */
    inline bool createInventoryPath(
        const std::string& associationPath, const std::string& sensorName,
        const uint16_t entityType, const uint16_t entityInstanceNum,
        const uint16_t containerId);

    /** @brief Emit state sensor event signal for state changes
     *
     * @param[in] sensorOffset - offset within composite sensor
     * @param[in] eventState - current event state
     * @param[in] previousEventState - previous event state
     */
    void emitStateSensorEvent(uint8_t sensorOffset, uint8_t eventState,
                              uint8_t previousEventState);

    /** @brief Parse and store possible states from PDR data
     *
     * @param[in] pdr - state sensor PDR structure
     */
    void parsePossibleStates(std::shared_ptr<pldm_state_sensor_pdr> pdr);

    /** @brief Terminus ID which the sensor belongs to */
    pldm_tid_t tid;

    /** @brief Sensor ID */
    uint16_t sensorId;

    /** @brief Sensor name */
    std::string sensorName;

    /** @brief D-Bus object path */
    std::string path;

    /** @brief Number of composite sensors */
    uint8_t compositeSensorCount;

    /** @brief Current state values for each composite sensor */
    std::vector<uint8_t> currentStates;

    /** @brief Previous state values for each composite sensor */
    std::vector<uint8_t> previousStates;

    /** @brief Possible states for each composite sensor
     * Map: sensorOffset -> set of possible state values
     */
    std::map<uint8_t, std::vector<uint8_t>> possibleStates;

    /** @brief State set IDs for each composite sensor */
    std::vector<uint16_t> stateSetIds;

    /** @brief Entity type from PDR */
    uint16_t entityType;

    /** @brief Entity instance from PDR */
    uint16_t entityInstance;

    /** @brief Container ID from PDR */
    uint16_t containerId;

    /** @brief D-Bus interfaces */
    std::unique_ptr<AvailabilityIntf> availabilityIntf = nullptr;
    std::unique_ptr<OperationalStatusIntf> operationalStatusIntf = nullptr;
    std::unique_ptr<AssociationDefinitionsIntf> associationDefinitionsIntf =
        nullptr;
    std::unique_ptr<EntityIntf> entityIntf = nullptr;

    /** @brief Sensor availability status */
    bool available = true;

    /** @brief Sensor functional status */
    bool functional = true;
};

} // namespace platform_mc
} // namespace pldm
