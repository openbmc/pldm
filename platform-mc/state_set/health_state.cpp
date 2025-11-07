#include "health_state.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <algorithm>
#include <regex>
#include <unordered_set>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

// =============================================================================
// Static Utility Method Implementations
// =============================================================================

std::string_view StateSetHealthState::healthStateToString(
    HealthStateValue state)
{
    switch (state)
    {
        case HealthStateValue::Normal:
            return "Normal";
        case HealthStateValue::NonCritical:
            return "Non-Critical";
        case HealthStateValue::Critical:
            return "Critical";
        case HealthStateValue::Fatal:
            return "Fatal";
        case HealthStateValue::UpperNonCritical:
            return "Upper Non-Critical";
        case HealthStateValue::LowerNonCritical:
            return "Lower Non-Critical";
        case HealthStateValue::UpperCritical:
            return "Upper Critical";
        case HealthStateValue::LowerCritical:
            return "Lower Critical";
        case HealthStateValue::UpperFatal:
            return "Upper Fatal";
        case HealthStateValue::LowerFatal:
            return "Lower Fatal";
        default:
            return "Unknown";
    }
}

std::optional<StateSetHealthState::HealthStateValue>
    StateSetHealthState::healthStateFromValue(uint8_t value)
{
    switch (value)
    {
        case PLDM_STATE_SET_HEALTH_STATE_NORMAL:
            return HealthStateValue::Normal;
        case PLDM_STATE_SET_HEALTH_STATE_NON_CRITICAL:
            return HealthStateValue::NonCritical;
        case PLDM_STATE_SET_HEALTH_STATE_CRITICAL:
            return HealthStateValue::Critical;
        case PLDM_STATE_SET_HEALTH_STATE_FATAL:
            return HealthStateValue::Fatal;
        case PLDM_STATE_SET_HEALTH_STATE_UPPER_NON_CRITICAL:
            return HealthStateValue::UpperNonCritical;
        case PLDM_STATE_SET_HEALTH_STATE_LOWER_NON_CRITICAL:
            return HealthStateValue::LowerNonCritical;
        case PLDM_STATE_SET_HEALTH_STATE_UPPER_CRITICAL:
            return HealthStateValue::UpperCritical;
        case PLDM_STATE_SET_HEALTH_STATE_LOWER_CRITICAL:
            return HealthStateValue::LowerCritical;
        case PLDM_STATE_SET_HEALTH_STATE_UPPER_FATAL:
            return HealthStateValue::UpperFatal;
        case PLDM_STATE_SET_HEALTH_STATE_LOWER_FATAL:
            return HealthStateValue::LowerFatal;
        default:
            return std::nullopt;
    }
}

bool StateSetHealthState::isValidHealthState(uint8_t value)
{
    static const std::unordered_set<uint8_t> validStates = {
        PLDM_STATE_SET_HEALTH_STATE_NORMAL,
        PLDM_STATE_SET_HEALTH_STATE_NON_CRITICAL,
        PLDM_STATE_SET_HEALTH_STATE_CRITICAL,
        PLDM_STATE_SET_HEALTH_STATE_FATAL,
        PLDM_STATE_SET_HEALTH_STATE_UPPER_NON_CRITICAL,
        PLDM_STATE_SET_HEALTH_STATE_LOWER_NON_CRITICAL,
        PLDM_STATE_SET_HEALTH_STATE_UPPER_CRITICAL,
        PLDM_STATE_SET_HEALTH_STATE_LOWER_CRITICAL,
        PLDM_STATE_SET_HEALTH_STATE_UPPER_FATAL,
        PLDM_STATE_SET_HEALTH_STATE_LOWER_FATAL};

    return validStates.find(value) != validStates.end();
}

bool StateSetHealthState::isHealthFaulted(HealthStateValue state)
{
    return state != HealthStateValue::Normal;
}

bool StateSetHealthState::isHealthCriticalOrWorse(HealthStateValue state)
{
    return state == HealthStateValue::Critical ||
           state == HealthStateValue::Fatal ||
           state == HealthStateValue::UpperCritical ||
           state == HealthStateValue::LowerCritical ||
           state == HealthStateValue::UpperFatal ||
           state == HealthStateValue::LowerFatal;
}

bool StateSetHealthState::isHealthFatal(HealthStateValue state)
{
    return state == HealthStateValue::Fatal ||
           state == HealthStateValue::UpperFatal ||
           state == HealthStateValue::LowerFatal;
}

uint8_t StateSetHealthState::getHealthSeverityLevel(HealthStateValue state)
{
    switch (state)
    {
        case HealthStateValue::Normal:
            return 0;
        case HealthStateValue::NonCritical:
        case HealthStateValue::UpperNonCritical:
        case HealthStateValue::LowerNonCritical:
            return 1;
        case HealthStateValue::Critical:
        case HealthStateValue::UpperCritical:
        case HealthStateValue::LowerCritical:
            return 2;
        case HealthStateValue::Fatal:
        case HealthStateValue::UpperFatal:
        case HealthStateValue::LowerFatal:
            return 3;
        default:
            return 0;
    }
}

bool StateSetHealthState::isHealthMoreSevere(HealthStateValue state1,
                                             HealthStateValue state2)
{
    return getHealthSeverityLevel(state1) > getHealthSeverityLevel(state2);
}

// =============================================================================
// State Set Implementation
// =============================================================================

StateSetHealthState::StateSetHealthState(uint8_t sensorOffset,
                                         const std::string& objectPath,
                                         const Associations& associations) :
    StateSet(PLDM_STATE_SET_HEALTH_STATE), sensorOffset(sensorOffset),
    objectPath(objectPath)
{
    try
    {
        createInterfaces(objectPath, associations);
        setDefaultValue();
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Failed to create Health State interfaces for {PATH}: {ERROR}",
            "PATH", objectPath, "ERROR", e);
        throw;
    }
}

void StateSetHealthState::createInterfaces(const std::string& path,
                                           const Associations& associations)
{
    auto& bus = pldm::utils::DBusHandler::getBus();

    // Create association definitions interface
    associationDefinitionsIntf =
        std::make_unique<AssociationDefinitionsIntf>(bus, path.c_str());
    associationDefinitionsIntf->associations(associations);

    // Create Availability interface
    availabilityIntf = std::make_unique<AvailabilityIntf>(bus, path.c_str());

    // Create OperationalStatus interface
    operationalStatusIntf =
        std::make_unique<OperationalStatusIntf>(bus, path.c_str());

    lg2::info("Created Health State D-Bus interfaces at {PATH}", "PATH", path);
}

void StateSetHealthState::setValue(uint8_t value)
{
    if (!availabilityIntf || !operationalStatusIntf)
    {
        lg2::error("D-Bus interfaces not initialized for {PATH}", "PATH",
                   objectPath.string());
        return;
    }

    // Validate using static utility method
    if (value != PLDM_SENSOR_UNKNOWN && value != PLDM_SENSOR_UNAVAILABLE &&
        !isValidHealthState(value))
    {
        lg2::warning("Invalid health state value {VALUE} for {PATH}", "VALUE",
                     value, "PATH", objectPath.string());
        return;
    }

    currentValue = value;
    bool available = mapPldmToAvailability(value);
    bool functional = mapPldmToFunctional(value);

    availabilityIntf->available(available);
    operationalStatusIntf->functional(functional);

    lg2::info("Health State updated for {PATH}: {STATE} ({VALUE}) -> Available:"
              "{AVAILABLE}, Functional:{FUNCTIONAL}",
              "PATH", objectPath.string(), "STATE", getStateValueName(),
              "VALUE", value, "AVAILABLE", available, "FUNCTIONAL", functional);
}

void StateSetHealthState::setDefaultValue()
{
    currentValue = PLDM_STATE_SET_HEALTH_STATE_NORMAL;
    if (availabilityIntf)
    {
        availabilityIntf->available(true);
    }
    if (operationalStatusIntf)
    {
        operationalStatusIntf->functional(true);
    }
}

bool StateSetHealthState::mapPldmToAvailability(uint8_t pldmState) const
{
    // Device is not available only in Fatal states
    // All other states: device is accessible/communicable
    switch (pldmState)
    {
        case PLDM_STATE_SET_HEALTH_STATE_FATAL:
        case PLDM_STATE_SET_HEALTH_STATE_UPPER_FATAL:
        case PLDM_STATE_SET_HEALTH_STATE_LOWER_FATAL:
            return false;

        default:
            // Normal, Non-Critical, Critical: device is available
            // Unknown/Unavailable: assume available
            return true;
    }
}

bool StateSetHealthState::mapPldmToFunctional(uint8_t pldmState) const
{
    // Device is functional only in Normal and Non-Critical states
    switch (pldmState)
    {
        case PLDM_STATE_SET_HEALTH_STATE_NORMAL:
        case PLDM_STATE_SET_HEALTH_STATE_NON_CRITICAL:
        case PLDM_STATE_SET_HEALTH_STATE_UPPER_NON_CRITICAL:
        case PLDM_STATE_SET_HEALTH_STATE_LOWER_NON_CRITICAL:
            return true;

        case PLDM_STATE_SET_HEALTH_STATE_CRITICAL:
        case PLDM_STATE_SET_HEALTH_STATE_UPPER_CRITICAL:
        case PLDM_STATE_SET_HEALTH_STATE_LOWER_CRITICAL:
        case PLDM_STATE_SET_HEALTH_STATE_FATAL:
        case PLDM_STATE_SET_HEALTH_STATE_UPPER_FATAL:
        case PLDM_STATE_SET_HEALTH_STATE_LOWER_FATAL:
            return false;

        default:
            // Unknown/Unavailable: assume functional
            return true;
    }
}

std::string StateSetHealthState::getStateValueName() const
{
    if (currentValue == PLDM_SENSOR_UNKNOWN)
    {
        return "Unknown";
    }
    if (currentValue == PLDM_SENSOR_UNAVAILABLE)
    {
        return "Unavailable";
    }

    // Use static utility for name conversion
    auto state = healthStateFromValue(currentValue);
    if (state.has_value())
    {
        return std::string(healthStateToString(state.value()));
    }

    return "Invalid";
}

std::tuple<std::string, std::string, Level, std::string, std::string>
    StateSetHealthState::getEventData() const
{
    // Generate Redfish ResourceEvent data based on current availability and
    // functional state
    bool available = availabilityIntf ? availabilityIntf->available() : true;
    bool functional =
        operationalStatusIntf ? operationalStatusIntf->functional() : true;

    // Determine severity based on state combination:
    // Available & Functional = OK
    // Available & !Functional = Critical (device accessible but not working)
    // !Available = Critical (device not accessible)

    if (!available || !functional)
    {
        return {std::string("ResourceEvent.1.0.ResourceStatusChangedCritical"),
                std::string("Critical"), Level::Critical, "", ""};
    }
    else
    {
        return {std::string("ResourceEvent.1.0.ResourceStatusChangedOK"),
                std::string("OK"), Level::Informational, "", ""};
    }
}

void StateSetHealthState::updateSensorName(const std::string& name)
{
    if (name == objectPath.filename())
    {
        return; // No change needed
    }

    // Update object path
    objectPath = objectPath.parent_path() / name;

    // Sanitize path for D-Bus
    auto path = std::regex_replace(objectPath.string(),
                                   std::regex("[^a-zA-Z0-9_/]+"), "_");

    auto& bus = pldm::utils::DBusHandler::getBus();

    // Recreate association definitions interface
    if (associationDefinitionsIntf)
    {
        auto associations = associationDefinitionsIntf->associations();
        associationDefinitionsIntf =
            std::make_unique<AssociationDefinitionsIntf>(bus, path.c_str());
        associationDefinitionsIntf->associations(associations);
    }

    // Recreate Availability interface with saved state
    if (availabilityIntf)
    {
        auto savedAvailable = availabilityIntf->available();
        availabilityIntf =
            std::make_unique<AvailabilityIntf>(bus, path.c_str());
        availabilityIntf->available(savedAvailable);
    }

    // Recreate OperationalStatus interface with saved state
    if (operationalStatusIntf)
    {
        auto savedFunctional = operationalStatusIntf->functional();
        operationalStatusIntf =
            std::make_unique<OperationalStatusIntf>(bus, path.c_str());
        operationalStatusIntf->functional(savedFunctional);
    }

    lg2::info("Updated Health State sensor name to {NAME} at {PATH}", "NAME",
              name, "PATH", path);
}

} // namespace platform_mc
} // namespace pldm
