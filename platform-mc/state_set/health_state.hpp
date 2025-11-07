#pragma once

#include "state_set.hpp"

#include <libpldm/state_set.h>

#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/State/Decorator/Availability/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace pldm
{
namespace platform_mc
{

using AvailabilityIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::State::Decorator::server::Availability>;
using OperationalStatusIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::State::
                                    Decorator::server::OperationalStatus>;

/**
 * @brief State Set implementation for Health State (DSP0249 ID=1)
 *
 * This class implements the Health State state set as defined in
 * DSP0249 Table 1. It maps PLDM health states to OpenBMC D-Bus interfaces:
 * - xyz.openbmc_project.State.Decorator.Availability (Available property)
 * - xyz.openbmc_project.State.Decorator.OperationalStatus (Functional property)
 *
 * Also provides static utility methods for Health State operations
 * that can be used without instantiating the class.
 */
class StateSetHealthState : public StateSet
{
  public:
    /**
     * @brief Health State enumeration following DSP0249 specification
     */
    enum class HealthStateValue : uint8_t
    {
        Normal = PLDM_STATE_SET_HEALTH_STATE_NORMAL,
        NonCritical = PLDM_STATE_SET_HEALTH_STATE_NON_CRITICAL,
        Critical = PLDM_STATE_SET_HEALTH_STATE_CRITICAL,
        Fatal = PLDM_STATE_SET_HEALTH_STATE_FATAL,
        UpperNonCritical = PLDM_STATE_SET_HEALTH_STATE_UPPER_NON_CRITICAL,
        LowerNonCritical = PLDM_STATE_SET_HEALTH_STATE_LOWER_NON_CRITICAL,
        UpperCritical = PLDM_STATE_SET_HEALTH_STATE_UPPER_CRITICAL,
        LowerCritical = PLDM_STATE_SET_HEALTH_STATE_LOWER_CRITICAL,
        UpperFatal = PLDM_STATE_SET_HEALTH_STATE_UPPER_FATAL,
        LowerFatal = PLDM_STATE_SET_HEALTH_STATE_LOWER_FATAL,
    };

    /**
     * @brief Construct a Health State state set
     *
     * @param[in] sensorOffset - Composite sensor offset
     * @param[in] objectPath - D-Bus object path
     * @param[in] associations - D-Bus associations
     */
    StateSetHealthState(uint8_t sensorOffset, const std::string& objectPath,
                        const Associations& associations);

    ~StateSetHealthState() override = default;

    // =========================================================================
    // StateSet Interface Implementation
    // =========================================================================

    /**
     * @brief Set the health state value (DSP0249)
     *
     * Maps PLDM health states to OpenBMC D-Bus interfaces:
     * Available (Availability):
     *   - Normal/Non-Critical/Critical: true (device accessible)
     *   - Fatal: false (device not accessible)
     * Functional (OperationalStatus):
     *   - Normal: true
     *   - Non-Critical: true (warning level, still functional)
     *   - Critical/Fatal: false (not functioning properly)
     *
     * @param[in] value - PLDM health state value (0-9)
     */
    void setValue(uint8_t value) override;

    /**
     * @brief Set default health state to Normal (Available=true,
     * Functional=true)
     */
    void setDefaultValue() override;

    /**
     * @brief Get state type name
     * @return "Health"
     */
    std::string getStateTypeName() const override
    {
        return "Health";
    }

    /**
     * @brief Get human-readable state value name
     * @return State name (e.g., "Normal", "Critical")
     */
    std::string getStateValueName() const override;

    /**
     * @brief Get event data for Redfish ResourceEvent
     *
     * Returns Redfish-compliant event data:
     * - ResourceEvent.1.0.ResourceStatusChangedOK
     * - ResourceEvent.1.0.ResourceStatusChangedWarning
     * - ResourceEvent.1.0.ResourceStatusChangedCritical
     *
     * @return tuple<messageId, message, level, resolution, additionalData>
     */
    std::tuple<std::string, std::string, Level, std::string, std::string>
        getEventData() const override;

    /**
     * @brief Update sensor name and recreate D-Bus objects
     * @param[in] name - New sensor name
     */
    void updateSensorName(const std::string& name) override;

    /**
     * @brief Get current availability state from D-Bus interface
     * @return bool - true if device is available
     */
    bool isAvailable() const
    {
        return availabilityIntf ? availabilityIntf->available() : true;
    }

    /**
     * @brief Get current functional state from D-Bus interface
     * @return bool - true if device is functional
     */
    bool isFunctional() const
    {
        return operationalStatusIntf ? operationalStatusIntf->functional()
                                     : true;
    }

    // =========================================================================
    // Static Utility Methods (DSP0249 Health State Helpers)
    // =========================================================================

    /**
     * @brief Get the PLDM state set ID for Health State
     * @return uint16_t - State set ID (1)
     */
    static constexpr uint16_t getHealthStateSetId()
    {
        return PLDM_STATE_SET_HEALTH_STATE;
    }

    /**
     * @brief Convert Health State enum to string representation
     * @param[in] state - Health state value
     * @return std::string_view - String representation of the state
     */
    static std::string_view healthStateToString(HealthStateValue state);

    /**
     * @brief Convert Health State enum to uint8_t
     * @param[in] state - Health state value
     * @return uint8_t - Raw state value
     */
    static constexpr uint8_t healthStateToValue(HealthStateValue state)
    {
        return static_cast<uint8_t>(state);
    }

    /**
     * @brief Convert uint8_t to Health State enum
     * @param[in] value - Raw state value
     * @return std::optional<HealthStateValue> - Health state enum if valid,
     * nullopt otherwise
     */
    static std::optional<HealthStateValue> healthStateFromValue(uint8_t value);

    /**
     * @brief Check if a state value is valid for Health State
     * @param[in] value - State value to validate
     * @return bool - true if valid, false otherwise
     */
    static bool isValidHealthState(uint8_t value);

    /**
     * @brief Check if the health state indicates a fault condition
     * @param[in] state - Health state to check
     * @return bool - true if state indicates fault (non-normal), false
     * otherwise
     */
    static bool isHealthFaulted(HealthStateValue state);

    /**
     * @brief Check if the health state is critical or worse
     * @param[in] state - Health state to check
     * @return bool - true if state is critical or fatal
     */
    static bool isHealthCriticalOrWorse(HealthStateValue state);

    /**
     * @brief Check if the health state is fatal
     * @param[in] state - Health state to check
     * @return bool - true if state is fatal
     */
    static bool isHealthFatal(HealthStateValue state);

    /**
     * @brief Get the severity level of a health state
     * @param[in] state - Health state
     * @return uint8_t - Severity level (0=Normal, 1=NonCritical, 2=Critical,
     * 3=Fatal)
     */
    static uint8_t getHealthSeverityLevel(HealthStateValue state);

    /**
     * @brief Compare two health states by severity
     * @param[in] state1 - First health state
     * @param[in] state2 - Second health state
     * @return bool - true if state1 is more severe than state2
     */
    static bool isHealthMoreSevere(HealthStateValue state1,
                                   HealthStateValue state2);

    /**
     * @brief Get all valid Health State values
     * @return std::array<HealthStateValue, 10> - Array of all valid states
     */
    static constexpr std::array<HealthStateValue, 10> getAllHealthStates()
    {
        return {HealthStateValue::Normal,
                HealthStateValue::NonCritical,
                HealthStateValue::Critical,
                HealthStateValue::Fatal,
                HealthStateValue::UpperNonCritical,
                HealthStateValue::LowerNonCritical,
                HealthStateValue::UpperCritical,
                HealthStateValue::LowerCritical,
                HealthStateValue::UpperFatal,
                HealthStateValue::LowerFatal};
    }

  private:
    /**
     * @brief Map PLDM health state to Availability (Available property)
     * @param[in] pldmState - PLDM health state value
     * @return bool - true if device is available/accessible
     */
    bool mapPldmToAvailability(uint8_t pldmState) const;

    /**
     * @brief Map PLDM health state to OperationalStatus (Functional property)
     * @param[in] pldmState - PLDM health state value
     * @return bool - true if device is functional
     */
    bool mapPldmToFunctional(uint8_t pldmState) const;

    /**
     * @brief Create D-Bus interfaces
     * @param[in] path - D-Bus object path
     * @param[in] associations - D-Bus associations
     */
    void createInterfaces(const std::string& path,
                          const Associations& associations);

    /** @brief Composite sensor offset */
    [[maybe_unused]] uint8_t sensorOffset;

    /** @brief D-Bus object path */
    std::filesystem::path objectPath;

    /** @brief Availability D-Bus interface */
    std::unique_ptr<AvailabilityIntf> availabilityIntf = nullptr;

    /** @brief OperationalStatus D-Bus interface */
    std::unique_ptr<OperationalStatusIntf> operationalStatusIntf = nullptr;
};

} // namespace platform_mc
} // namespace pldm
