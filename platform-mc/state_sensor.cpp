#include "state_sensor.hpp"

#include "common/utils.hpp"

#include <libpldm/platform.h>
#include <libpldm/states.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/property.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <bitset>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

inline bool StateSensor::createInventoryPath(
    const std::string& associationPath, const std::string& sensorName,
    const uint16_t entityType, const uint16_t entityInstanceNum,
    const uint16_t containerId)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::string invPath = associationPath + "/" + sensorName;

    try
    {
        entityIntf = std::make_unique<EntityIntf>(bus, invPath.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create Entity interface for state sensor {PATH} error - {ERROR}",
            "PATH", invPath, "ERROR", e);
        return false;
    }

    entityIntf->entityType(entityType);
    entityIntf->entityInstanceNumber(entityInstanceNum);
    entityIntf->containerID(containerId);

    return true;
}

void StateSensor::parsePossibleStates(
    std::shared_ptr<pldm_state_sensor_pdr> pdr)
{
    if (!pdr)
    {
        lg2::error("Invalid PDR provided to parsePossibleStates");
        return;
    }

    uint8_t* statePtr = pdr->possible_states;

    for (uint8_t i = 0; i < compositeSensorCount; i++)
    {
        auto* possibleStatesStruct =
            reinterpret_cast<state_sensor_possible_states*>(statePtr);

        uint16_t stateSetId = possibleStatesStruct->state_set_id;
        uint8_t possibleStatesSize = possibleStatesStruct->possible_states_size;

        stateSetIds.push_back(stateSetId);

        // Parse possible states from bitfield
        std::vector<uint8_t> states;
        uint8_t* statesData = statePtr + sizeof(state_sensor_possible_states);

        for (uint8_t byteIdx = 0; byteIdx < possibleStatesSize; byteIdx++)
        {
            uint8_t byte = statesData[byteIdx];
            for (uint8_t bitIdx = 0; bitIdx < 8; bitIdx++)
            {
                if (byte & (1 << bitIdx))
                {
                    uint8_t state = (byteIdx * 8) + bitIdx;
                    states.push_back(state);
                }
            }
        }

        possibleStates[i] = std::move(states);

        // Move to next sensor's possible states
        if (possibleStatesSize > 0)
        {
            statePtr += sizeof(state_sensor_possible_states) +
                        possibleStatesSize - 1;
        }
        else
        {
            statePtr += sizeof(state_sensor_possible_states);
        }
    }
}

StateSensor::StateSensor(const pldm_tid_t tid, const bool sensorDisabled,
                         std::shared_ptr<pldm_state_sensor_pdr> pdr,
                         const std::string& sensorName,
                         const std::string& associationPath) :
    tid(tid), sensorName(sensorName)
{
    if (sensorDisabled)
    {
        lg2::info("State sensor {NAME} is disabled", "NAME", sensorName);
    }

    if (!pdr)
    {
        lg2::error("Invalid state sensor PDR provided");
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    sensorId = pdr->sensor_id;
    compositeSensorCount = pdr->composite_sensor_count;
    entityType = pdr->entity_type;
    entityInstance = pdr->entity_instance;
    containerId = pdr->container_id;

    // Initialize state vectors
    currentStates.resize(compositeSensorCount, PLDM_SENSOR_UNKNOWN);
    previousStates.resize(compositeSensorCount, PLDM_SENSOR_UNKNOWN);

    // Parse possible states from PDR
    parsePossibleStates(pdr);

    // Create D-Bus path
    path = "/xyz/openbmc_project/sensors/state/" + sensorName;

    try
    {
        // Check if D-Bus path already exists
        std::string tmp{};
        tmp = pldm::utils::DBusHandler().getService(path.c_str(),
                                                    STATE_SENSOR_VALUE_INTF);

        if (!tmp.empty())
        {
            lg2::error("State sensor D-Bus path {PATH} already exists", "PATH",
                       path);
            throw sdbusplus::xyz::openbmc_project::Common::Error::
                TooManyResources();
        }
    }
    catch (const sdbusplus::exception_t&)
    {
        // Expected when service doesn't exist yet
    }

    try
    {
        auto& bus = pldm::utils::DBusHandler::getBus();

        // Create availability interface
        availabilityIntf =
            std::make_unique<AvailabilityIntf>(bus, path.c_str());
        availabilityIntf->available(true);

        // Create operational status interface
        operationalStatusIntf =
            std::make_unique<OperationalStatusIntf>(bus, path.c_str());
        operationalStatusIntf->functional(true);

        // Create association definitions interface
        associationDefinitionsIntf =
            std::make_unique<AssociationDefinitionsIntf>(bus, path.c_str());

        // Create inventory path if association path is provided
        if (!associationPath.empty())
        {
            if (!createInventoryPath(associationPath, sensorName, entityType,
                                     entityInstance, containerId))
            {
                lg2::warning(
                    "Failed to create inventory path for state sensor {NAME}",
                    "NAME", sensorName);
            }
        }

        lg2::info("Created state sensor {NAME} with {COUNT} composite sensors",
                  "NAME", sensorName, "COUNT", compositeSensorCount);
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create D-Bus interfaces for state sensor {NAME} error - {ERROR}",
            "NAME", sensorName, "ERROR", e);
        throw;
    }
}

void StateSensor::handleErrGetSensorReading()
{
    lg2::error("Error getting state sensor reading for sensor {NAME}", "NAME",
               sensorName);

    if (operationalStatusIntf)
    {
        operationalStatusIntf->functional(false);
    }

    if (availabilityIntf)
    {
        availabilityIntf->available(false);
    }

    functional = false;
    available = false;
}

void StateSensor::updateReading(bool available, bool functional,
                                const std::vector<uint8_t>& sensorStates)
{
    this->available = available;
    this->functional = functional;

    if (!availabilityIntf || !operationalStatusIntf)
    {
        lg2::error(
            "Failed to update state sensor {NAME} - D-Bus interfaces don't exist.",
            "NAME", sensorName);
        return;
    }

    availabilityIntf->available(available);
    operationalStatusIntf->functional(functional);

    if (!available || !functional)
    {
        // Set all states to unknown when sensor is not available/functional
        for (uint8_t i = 0; i < compositeSensorCount; i++)
        {
            if (currentStates[i] != PLDM_SENSOR_UNKNOWN)
            {
                previousStates[i] = currentStates[i];
                currentStates[i] = PLDM_SENSOR_UNKNOWN;

                emitStateSensorEvent(i, PLDM_SENSOR_UNKNOWN, previousStates[i]);
            }
        }
        return;
    }

    // Update states and emit events for changes
    uint8_t numStates = std::min(static_cast<uint8_t>(sensorStates.size()),
                                 compositeSensorCount);

    for (uint8_t i = 0; i < numStates; i++)
    {
        uint8_t newState = sensorStates[i];

        // Validate state is supported
        if (!isStateSupported(i, newState))
        {
            lg2::warning(
                "Unsupported state {STATE} for sensor {NAME} offset {OFFSET}",
                "STATE", newState, "NAME", sensorName, "OFFSET", i);
            newState = PLDM_SENSOR_UNKNOWN;
        }

        if (currentStates[i] != newState)
        {
            previousStates[i] = currentStates[i];
            currentStates[i] = newState;

            // Emit state change event
            emitStateSensorEvent(i, newState, previousStates[i]);

            lg2::debug(
                "State sensor {NAME} offset {OFFSET} changed from {PREV} to {CURR}",
                "NAME", sensorName, "OFFSET", i, "PREV", previousStates[i],
                "CURR", newState);
        }
    }
}

uint8_t StateSensor::getCurrentState(uint8_t sensorOffset) const
{
    if (sensorOffset >= currentStates.size())
    {
        lg2::error("Invalid sensor offset {OFFSET} for sensor {NAME}", "OFFSET",
                   sensorOffset, "NAME", sensorName);
        return PLDM_SENSOR_UNKNOWN;
    }

    return currentStates[sensorOffset];
}

uint8_t StateSensor::getPreviousState(uint8_t sensorOffset) const
{
    if (sensorOffset >= previousStates.size())
    {
        lg2::error("Invalid sensor offset {OFFSET} for sensor {NAME}", "OFFSET",
                   sensorOffset, "NAME", sensorName);
        return PLDM_SENSOR_UNKNOWN;
    }

    return previousStates[sensorOffset];
}

bool StateSensor::isStateSupported(uint8_t sensorOffset, uint8_t state) const
{
    if (sensorOffset >= compositeSensorCount)
    {
        return false;
    }

    // UNKNOWN and UNAVAILABLE are always supported
    if (state == PLDM_SENSOR_UNKNOWN || state == PLDM_SENSOR_UNAVAILABLE)
    {
        return true;
    }

    auto it = possibleStates.find(sensorOffset);
    if (it == possibleStates.end())
    {
        return false;
    }

    const auto& states = it->second;
    return std::find(states.begin(), states.end(), state) != states.end();
}

void StateSensor::emitStateSensorEvent(uint8_t sensorOffset, uint8_t eventState,
                                       uint8_t previousEventState)
{
    try
    {
        // Use the common utility function to emit state sensor event
        pldm::utils::emitStateSensorEventSignal(tid, sensorId, sensorOffset,
                                                eventState, previousEventState);

        lg2::debug(
            "Emitted state sensor event for TID {TID} sensor {SID} offset {OFFSET}",
            "TID", tid, "SID", sensorId, "OFFSET", sensorOffset);
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Failed to emit state sensor event for sensor {NAME} error - {ERROR}",
            "NAME", sensorName, "ERROR", e);
    }
}

} // namespace platform_mc
} // namespace pldm
