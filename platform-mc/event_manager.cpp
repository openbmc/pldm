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
            lg2::error("Failed to decode sensor event data return code {RC}.",
                       "RC", rc);
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
                lg2::info("Unsupported class type {CLASSTYPE}", "CLASSTYPE",
                          sensorEventClassType);
                return PLDM_ERROR;
        }
    }

    lg2::info("Unsupported class type {CLASSTYPE}", "CLASSTYPE", eventClass);

    return PLDM_ERROR;
}

int EventManager::createSensorThresholdLogEntry(
    const std::string& messageId, const std::string& sensorName,
    const double reading, const double threshold)
{
    using namespace sdbusplus::xyz::openbmc_project::Logging::server;
    using Level =
        sdbusplus::xyz::openbmc_project::Logging::server::Entry::Level;

    auto createLog = [&messageId](std::map<std::string, std::string>& addData,
                                  Level& level) {
        static constexpr auto logObjPath = "/xyz/openbmc_project/logging";
        static constexpr auto logInterface =
            "xyz.openbmc_project.Logging.Create";
        auto& bus = pldm::utils::DBusHandler::getBus();

        try
        {
            auto service =
                pldm::utils::DBusHandler().getService(logObjPath, logInterface);
            auto severity = sdbusplus::xyz::openbmc_project::Logging::server::
                convertForMessage(level);
            auto method = bus.new_method_call(service.c_str(), logObjPath,
                                              logInterface, "Create");
            method.append(messageId, severity, addData);
            bus.call_noreply(method);
        }
        catch (const std::exception& e)
        {
            lg2::error(
                "Failed to create D-Bus log entry for message registry, error - {ERROR}.",
                "ERROR", e);
        }
    };

    std::map<std::string, std::string> addData;
    addData["REDFISH_MESSAGE_ID"] = messageId;
    Level level = Level::Informational;

    addData["REDFISH_MESSAGE_ARGS"] =
        sensorName + "," + std::to_string(reading) + "," +
        std::to_string(threshold);

    if (messageId == pldm::platform_mc::SensorThresholdWarningLowGoingHigh ||
        messageId == pldm::platform_mc::SensorThresholdWarningHighGoingLow)
    {
        level = Level::Informational;
    }
    else if (messageId ==
                 pldm::platform_mc::SensorThresholdWarningLowGoingLow ||
             messageId ==
                 pldm::platform_mc::SensorThresholdWarningHighGoingHigh ||
             messageId ==
                 pldm::platform_mc::SensorThresholdCriticalLowGoingHigh ||
             messageId ==
                 pldm::platform_mc::SensorThresholdCriticalHighGoingLow)
    {
        level = Level::Warning;
    }
    else if (messageId ==
                 pldm::platform_mc::SensorThresholdCriticalLowGoingLow ||
             messageId ==
                 pldm::platform_mc::SensorThresholdCriticalHighGoingHigh)
    {
        level = Level::Critical;
    }
    else
    {
        /* OEM message. Will be handle by OEM */
        return PLDM_ERROR;
    }

    createLog(addData, level);
    return PLDM_SUCCESS;
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

    for (auto& [terminusId, terminus] : termini)
    {
        if (tid != terminusId)
        {
            continue;
        }
        for (auto& sensor : terminus->numericSensors)
        {
            if (sensorId != sensor->sensorId)
            {
                continue;
            }
            std::string messageId =
                getSensorThresholdMessageId(previousEventState, eventState);
            double threshold = std::numeric_limits<double>::quiet_NaN();
            double reading = std::numeric_limits<double>::quiet_NaN();
            switch (eventState)
            {
                case PLDM_SENSOR_LOWERFATAL:
                case PLDM_SENSOR_LOWERCRITICAL:
                    threshold = sensor->getThresholdLowerCritical();
                    break;
                case PLDM_SENSOR_UPPERFATAL:
                case PLDM_SENSOR_UPPERCRITICAL:
                    threshold = sensor->getThresholdUpperCritical();
                    break;
                case PLDM_SENSOR_LOWERWARNING:
                    threshold = sensor->getThresholdLowerWarning();
                    break;
                case PLDM_SENSOR_UPPERWARNING:
                    threshold = sensor->getThresholdUpperWarning();
                    break;
                default:
                    break;
            }
            switch (sensorDataSize)
            {
                case PLDM_SENSOR_DATA_SIZE_UINT8:
                    reading = static_cast<double>(
                        static_cast<uint8_t>(presentReading));
                    break;
                case PLDM_SENSOR_DATA_SIZE_SINT8:
                    reading = static_cast<double>(
                        static_cast<int8_t>(presentReading));
                    break;
                case PLDM_SENSOR_DATA_SIZE_UINT16:
                    reading = static_cast<double>(
                        static_cast<uint16_t>(presentReading));
                    break;
                case PLDM_SENSOR_DATA_SIZE_SINT16:
                    reading = static_cast<double>(
                        static_cast<int16_t>(presentReading));
                    break;
                case PLDM_SENSOR_DATA_SIZE_UINT32:
                    reading = static_cast<double>(
                        static_cast<uint32_t>(presentReading));
                    break;
                case PLDM_SENSOR_DATA_SIZE_SINT32:
                    reading = static_cast<double>(
                        static_cast<int32_t>(presentReading));
                    break;
                default:
                    break;
            }
            rc = createSensorThresholdLogEntry(
                messageId, sensor->sensorName,
                sensor->unitModifier(sensor->conversionFormula(reading)),
                threshold);
            return rc;
        }
    }
    return PLDM_ERROR;
}

std::string EventManager::getSensorThresholdMessageId(
    uint8_t previousEventState, uint8_t eventState)
{
    switch (previousEventState)
    {
        case PLDM_SENSOR_UPPERFATAL:
        case PLDM_SENSOR_UPPERCRITICAL:
            switch (eventState)
            {
                case PLDM_SENSOR_UPPERFATAL:
                case PLDM_SENSOR_UPPERCRITICAL:
                    return pldm::platform_mc::
                        SensorThresholdCriticalHighGoingHigh;
                case PLDM_SENSOR_UPPERWARNING:
                    return pldm::platform_mc::
                        SensorThresholdCriticalHighGoingLow;
                case PLDM_SENSOR_NORMAL:
                    return pldm::platform_mc::
                        SensorThresholdWarningHighGoingLow;
                case PLDM_SENSOR_LOWERWARNING:
                    return pldm::platform_mc::SensorThresholdWarningLowGoingLow;
                case PLDM_SENSOR_LOWERCRITICAL:
                case PLDM_SENSOR_LOWERFATAL:
                    return pldm::platform_mc::
                        SensorThresholdCriticalLowGoingLow;
                default:
                    break;
            }
            break;
        case PLDM_SENSOR_UPPERWARNING:
            switch (eventState)
            {
                case PLDM_SENSOR_UPPERFATAL:
                case PLDM_SENSOR_UPPERCRITICAL:
                    return pldm::platform_mc::
                        SensorThresholdCriticalHighGoingHigh;
                case PLDM_SENSOR_UPPERWARNING:
                    return pldm::platform_mc::
                        SensorThresholdWarningHighGoingHigh;
                case PLDM_SENSOR_NORMAL:
                    return pldm::platform_mc::
                        SensorThresholdWarningHighGoingLow;
                case PLDM_SENSOR_LOWERWARNING:
                    return pldm::platform_mc::SensorThresholdWarningLowGoingLow;
                case PLDM_SENSOR_LOWERCRITICAL:
                case PLDM_SENSOR_LOWERFATAL:
                    return pldm::platform_mc::
                        SensorThresholdCriticalLowGoingLow;
                default:
                    break;
            }
            break;
        case PLDM_SENSOR_UNKNOWN:
        case PLDM_SENSOR_NORMAL:
            switch (eventState)
            {
                case PLDM_SENSOR_UPPERFATAL:
                case PLDM_SENSOR_UPPERCRITICAL:
                    return pldm::platform_mc::
                        SensorThresholdCriticalHighGoingHigh;
                case PLDM_SENSOR_UPPERWARNING:
                    return pldm::platform_mc::
                        SensorThresholdWarningHighGoingHigh;
                case PLDM_SENSOR_NORMAL:
                    break;
                case PLDM_SENSOR_LOWERWARNING:
                    return pldm::platform_mc::SensorThresholdWarningLowGoingLow;
                case PLDM_SENSOR_LOWERCRITICAL:
                case PLDM_SENSOR_LOWERFATAL:
                    return pldm::platform_mc::
                        SensorThresholdCriticalLowGoingLow;
                default:
                    break;
            }
            break;
        case PLDM_SENSOR_LOWERWARNING:
            switch (eventState)
            {
                case PLDM_SENSOR_UPPERFATAL:
                case PLDM_SENSOR_UPPERCRITICAL:
                    return pldm::platform_mc::
                        SensorThresholdCriticalHighGoingHigh;
                case PLDM_SENSOR_UPPERWARNING:
                    return pldm::platform_mc::
                        SensorThresholdWarningHighGoingHigh;
                case PLDM_SENSOR_NORMAL:
                    return pldm::platform_mc::
                        SensorThresholdWarningLowGoingHigh;
                case PLDM_SENSOR_LOWERWARNING:
                    return pldm::platform_mc::SensorThresholdWarningLowGoingLow;
                case PLDM_SENSOR_LOWERCRITICAL:
                case PLDM_SENSOR_LOWERFATAL:
                    return pldm::platform_mc::
                        SensorThresholdCriticalLowGoingLow;
                default:
                    break;
            }
            break;
        case PLDM_SENSOR_LOWERCRITICAL:
        case PLDM_SENSOR_LOWERFATAL:
            switch (eventState)
            {
                case PLDM_SENSOR_UPPERFATAL:
                case PLDM_SENSOR_UPPERCRITICAL:
                    return pldm::platform_mc::
                        SensorThresholdCriticalHighGoingHigh;
                case PLDM_SENSOR_UPPERWARNING:
                    return pldm::platform_mc::
                        SensorThresholdWarningHighGoingHigh;
                case PLDM_SENSOR_NORMAL:
                    return pldm::platform_mc::
                        SensorThresholdWarningLowGoingHigh;
                case PLDM_SENSOR_LOWERWARNING:
                    return pldm::platform_mc::
                        SensorThresholdCriticalLowGoingHigh;
                case PLDM_SENSOR_LOWERCRITICAL:
                case PLDM_SENSOR_LOWERFATAL:
                    return pldm::platform_mc::
                        SensorThresholdCriticalLowGoingLow;
                default:
                    break;
            }
            break;
    }
    return std::string{};
}

} // namespace platform_mc
} // namespace pldm
