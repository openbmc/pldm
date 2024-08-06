#include "event_manager.hpp"

#include "libpldm/utils.h"

#include "terminus_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <cerrno>
#include <memory>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{
namespace fs = std::filesystem;

int EventManager::handlePlatformEvent(
    pldm_tid_t tid, uint16_t eventId, uint8_t eventClass,
    const uint8_t* eventData, size_t eventDataSize)
{
    /* EventClass sensorEvent `Table 11 - PLDM Event Types` DSP0248 */
    if (eventClass == PLDM_SENSOR_EVENT)
    {
        uint16_t sensorId = 0;
        uint8_t sensorEventClassType = 0;
        size_t eventClassDataOffset = 0;
        auto rc = decode_sensor_event_data(eventData, eventDataSize, &sensorId,
                                           &sensorEventClassType,
                                           &eventClassDataOffset);
        if (rc)
        {
            lg2::error(
                "Failed to decode sensor event data from terminus ID {TID}, event class {CLASS}, event ID {EVENTID} with return code {RC}.",
                "TID", tid, "CLASS", eventClass, "EVENTID", eventId, "RC", rc);
            return rc;
        }
        switch (sensorEventClassType)
        {
            case PLDM_NUMERIC_SENSOR_STATE:
            {
                const uint8_t* sensorData = eventData + eventClassDataOffset;
                size_t sensorDataLength = eventDataSize - eventClassDataOffset;
                return processNumericSensorEvent(tid, sensorId, sensorData,
                                                 sensorDataLength);
            }
            case PLDM_STATE_SENSOR_STATE:
            case PLDM_SENSOR_OP_STATE:
            default:
                lg2::info(
                    "Unsupported class type {CLASSTYPE} for the sensor event from terminus ID {TID} sensorId {SID}",
                    "CLASSTYPE", sensorEventClassType, "TID", tid, "SID",
                    sensorId);
                return PLDM_ERROR;
        }
    }

    lg2::info("Unsupported class type {CLASSTYPE}", "CLASSTYPE", eventClass);

    return PLDM_ERROR;
}

int EventManager::processNumericSensorEvent(pldm_tid_t tid, uint16_t sensorId,
                                            const uint8_t* sensorData,
                                            size_t sensorDataLength)
{
    uint8_t eventState = 0;
    uint8_t previousEventState = 0;
    uint8_t sensorDataSize = 0;
    uint32_t presentReading;
    auto rc = decode_numeric_sensor_data(
        sensorData, sensorDataLength, &eventState, &previousEventState,
        &sensorDataSize, &presentReading);
    if (rc)
    {
        lg2::error(
            "Failed to decode numericSensorState event for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        return rc;
    }

    double value = static_cast<double>(presentReading);
    lg2::error(
        "processNumericSensorEvent tid {TID}, sensorID {SID} value {VAL} previousState {PSTATE} eventState {ESTATE}",
        "TID", tid, "SID", sensorId, "VAL", value, "PSTATE", previousEventState,
        "ESTATE", eventState);

    if (!termini.contains(tid) || !termini[tid])
    {
        lg2::error("Terminus ID {TID} is not in the managing list.", "TID",
                   tid);
        return PLDM_ERROR;
    }

    auto& terminus = termini[tid];

    auto sensor = terminus->getSensorObject(sensorId);
    if (!sensor)
    {
        lg2::error(
            "Terminus ID {TID} has no sensor object with sensor ID {SID}.",
            "TID", tid, "SID", sensorId);
        return PLDM_ERROR;
    }

    switch (previousEventState)
    {
        case PLDM_SENSOR_UNKNOWN:
        case PLDM_SENSOR_NORMAL:
        {
            switch (eventState)
            {
                case PLDM_SENSOR_UPPERFATAL:
                case PLDM_SENSOR_UPPERCRITICAL:
                {
                    sensor->triggerThresholdEvent(pldm::utils::Level::WARNING,
                                                  pldm::utils::Direction::HIGH,
                                                  value, true, true);
                    return sensor->triggerThresholdEvent(
                        pldm::utils::Level::CRITICAL,
                        pldm::utils::Direction::HIGH, value, true, true);
                }
                case PLDM_SENSOR_UPPERWARNING:
                {
                    return sensor->triggerThresholdEvent(
                        pldm::utils::Level::WARNING,
                        pldm::utils::Direction::HIGH, value, true, true);
                }
                case PLDM_SENSOR_NORMAL:
                    break;
                case PLDM_SENSOR_LOWERWARNING:
                {
                    return sensor->triggerThresholdEvent(
                        pldm::utils::Level::WARNING,
                        pldm::utils::Direction::LOW, value, true, true);
                }
                case PLDM_SENSOR_LOWERCRITICAL:
                case PLDM_SENSOR_LOWERFATAL:
                {
                    sensor->triggerThresholdEvent(pldm::utils::Level::WARNING,
                                                  pldm::utils::Direction::LOW,
                                                  value, true, true);
                    return sensor->triggerThresholdEvent(
                        pldm::utils::Level::CRITICAL,
                        pldm::utils::Direction::LOW, value, true, true);
                }
                default:
                    break;
            }
            break;
        }
        case PLDM_SENSOR_LOWERWARNING:
        {
            switch (eventState)
            {
                case PLDM_SENSOR_UPPERFATAL:
                case PLDM_SENSOR_UPPERCRITICAL:
                    break;
                case PLDM_SENSOR_UPPERWARNING:
                {
                    sensor->triggerThresholdEvent(pldm::utils::Level::WARNING,
                                                  pldm::utils::Direction::LOW,
                                                  value, false, false);
                    return sensor->triggerThresholdEvent(
                        pldm::utils::Level::WARNING,
                        pldm::utils::Direction::HIGH, value, true, true);
                }
                case PLDM_SENSOR_NORMAL:
                {
                    return sensor->triggerThresholdEvent(
                        pldm::utils::Level::WARNING,
                        pldm::utils::Direction::LOW, value, false, false);
                }
                case PLDM_SENSOR_LOWERWARNING:
                    break;
                case PLDM_SENSOR_LOWERCRITICAL:
                case PLDM_SENSOR_LOWERFATAL:
                {
                    return sensor->triggerThresholdEvent(
                        pldm::utils::Level::CRITICAL,
                        pldm::utils::Direction::LOW, value, true, true);
                }
                default:
                    break;
            }
            break;
        }
        case PLDM_SENSOR_LOWERCRITICAL:
        case PLDM_SENSOR_LOWERFATAL:
        {
            switch (eventState)
            {
                case PLDM_SENSOR_UPPERFATAL:
                case PLDM_SENSOR_UPPERCRITICAL:
                case PLDM_SENSOR_UPPERWARNING:
                    break;
                case PLDM_SENSOR_NORMAL:
                {
                    sensor->triggerThresholdEvent(pldm::utils::Level::CRITICAL,
                                                  pldm::utils::Direction::LOW,
                                                  value, false, false);
                    sensor->triggerThresholdEvent(pldm::utils::Level::WARNING,
                                                  pldm::utils::Direction::LOW,
                                                  value, true, true);
                    return sensor->triggerThresholdEvent(
                        pldm::utils::Level::WARNING,
                        pldm::utils::Direction::LOW, value, false, false);
                }
                case PLDM_SENSOR_LOWERWARNING:
                {
                    sensor->triggerThresholdEvent(pldm::utils::Level::CRITICAL,
                                                  pldm::utils::Direction::LOW,
                                                  value, false, false);
                    return sensor->triggerThresholdEvent(
                        pldm::utils::Level::WARNING,
                        pldm::utils::Direction::LOW, value, true, true);
                }
                case PLDM_SENSOR_LOWERCRITICAL:
                case PLDM_SENSOR_LOWERFATAL:
                default:
                    break;
            }
            break;
        }
        case PLDM_SENSOR_UPPERFATAL:
        case PLDM_SENSOR_UPPERCRITICAL:
        {
            switch (eventState)
            {
                case PLDM_SENSOR_UPPERFATAL:
                case PLDM_SENSOR_UPPERCRITICAL:
                    break;
                case PLDM_SENSOR_UPPERWARNING:
                {
                    sensor->triggerThresholdEvent(pldm::utils::Level::CRITICAL,
                                                  pldm::utils::Direction::HIGH,
                                                  value, false, false);
                    return sensor->triggerThresholdEvent(
                        pldm::utils::Level::WARNING,
                        pldm::utils::Direction::HIGH, value, true, true);
                }
                case PLDM_SENSOR_NORMAL:
                {
                    sensor->triggerThresholdEvent(pldm::utils::Level::CRITICAL,
                                                  pldm::utils::Direction::HIGH,
                                                  value, false, false);
                    sensor->triggerThresholdEvent(pldm::utils::Level::WARNING,
                                                  pldm::utils::Direction::HIGH,
                                                  value, true, true);
                    return sensor->triggerThresholdEvent(
                        pldm::utils::Level::WARNING,
                        pldm::utils::Direction::HIGH, value, false, false);
                }
                case PLDM_SENSOR_LOWERWARNING:
                case PLDM_SENSOR_LOWERCRITICAL:
                case PLDM_SENSOR_LOWERFATAL:
                default:
                    break;
            }
            break;
        }
        case PLDM_SENSOR_UPPERWARNING:
        {
            switch (eventState)
            {
                case PLDM_SENSOR_UPPERFATAL:
                case PLDM_SENSOR_UPPERCRITICAL:
                {
                    return sensor->triggerThresholdEvent(
                        pldm::utils::Level::CRITICAL,
                        pldm::utils::Direction::HIGH, value, true, true);
                }
                case PLDM_SENSOR_UPPERWARNING:
                    break;
                case PLDM_SENSOR_NORMAL:
                {
                    return sensor->triggerThresholdEvent(
                        pldm::utils::Level::WARNING,
                        pldm::utils::Direction::HIGH, value, false, false);
                }
                case PLDM_SENSOR_LOWERWARNING:
                {
                    sensor->triggerThresholdEvent(pldm::utils::Level::WARNING,
                                                  pldm::utils::Direction::HIGH,
                                                  value, false, false);
                    return sensor->triggerThresholdEvent(
                        pldm::utils::Level::WARNING,
                        pldm::utils::Direction::LOW, value, true, true);
                }
                case PLDM_SENSOR_LOWERCRITICAL:
                case PLDM_SENSOR_LOWERFATAL:
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }

    return PLDM_SUCCESS;
}

} // namespace platform_mc
} // namespace pldm
