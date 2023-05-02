#include "event_manager.hpp"

#include "libpldm/utils.h"

#include "terminus_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <cerrno>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{
namespace fs = std::filesystem;

/** @brief PLDM cperEventClass formatType
 */
enum class CperFormatType
{
    PLDM_FORMAT_TYPE_CPER = 0x0,
    PLDM_FORMAT_TYPE_CPER_SECTION = 0x1
};

/* Minumum length of pldm cper event data */
#define PLDM_CPER_EVENT_DATA_MIN_LENGTH 4

/** @struct pldm_cper_event
 *
 *  structure representing CPEREvent
 */
struct pldm_cper_event_data
{
    uint8_t format_version;
    uint8_t format_type;
    uint16_t event_data_length;
    uint8_t event_data[1];
} __attribute__((packed));

/** @brief Decode cperEvent response data
 *
 *  @param[in] event_data - event data from the response message
 *  @param[in] event_data_length - length of the event data
 *  @param[out] format_version - version of the event format
 *  @param[out] format_type - 0x00=CPER(full record), 0x01=CPER Section(signal
 * CPER section)
 *  @param[out] cper_event_data_length - length in bytes of cper_event_data
 *  @param[out] cper_event_data - the pointer to where cper data is in
 * event_data array
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'event_data'
 */
int decode_pldm_cper_event_data(const uint8_t* event_data,
                                size_t event_data_length,
                                uint8_t* format_version, uint8_t* format_type,
                                uint16_t* cper_event_data_length,
                                uint8_t** cper_event_data)
{
    if (!event_data || !format_version || !format_type ||
        !cper_event_data_length || !cper_event_data)
    {
        return PLDM_ERROR_INVALID_DATA;
    }

    if (event_data_length < PLDM_CPER_EVENT_DATA_MIN_LENGTH)
    {
        return PLDM_ERROR_INVALID_LENGTH;
    }

    struct pldm_cper_event_data* pldm_cper_event =
        (struct pldm_cper_event_data*)event_data;
    *format_version = pldm_cper_event->format_version;
    *format_type = pldm_cper_event->format_type;
    *cper_event_data_length = le16toh(pldm_cper_event->event_data_length);
    *cper_event_data = pldm_cper_event->event_data;

    return PLDM_SUCCESS;
}

int EventManager::handlePlatformEvent(pldm_tid_t tid, uint16_t eventId,
                                      uint8_t eventClass,
                                      const uint8_t* eventData,
                                      size_t eventDataSize)
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
    /* EventClass CPER (0xFA OEM) `Table 11 - PLDM Event Types` DSP0248 */
    else if (eventClass == PLDM_OEM_EVENT_CLASS_0xFA)
    {
        return processCperEvent(eventId, eventData, eventDataSize);
    }

    lg2::info("Unsupported class type {CLASSTYPE}", "CLASSTYPE", eventClass);

    return PLDM_ERROR;
}

int EventManager::processCperEvent([[maybe_unused]] uint16_t eventId,
                                   const uint8_t* eventData,
                                   size_t eventDataSize)
{
    uint8_t formatVersion;
    uint8_t formatType = 0;
    uint16_t cperEventDataLength;
    uint8_t* cperEventData;
    auto rc = decode_pldm_cper_event_data(eventData, eventDataSize,
                                          &formatVersion, &formatType,
                                          &cperEventDataLength, &cperEventData);

    if (rc)
    {
        lg2::error("Failed to decode CPER event, error {RC} ", "RC", rc);
        return rc;
    }

    // save event data to file
    std::string dirName{"/var/cper"};
    auto dirStatus = fs::status(dirName);
    if (fs::exists(dirStatus))
    {
        if (!fs::is_directory(dirStatus))
        {
            lg2::error("Failed to create '{DIRNAME}' directory", "DIRNAME",
                       dirName);
            return PLDM_ERROR;
        }
    }
    else
    {
        fs::create_directory(dirName);
    }

    std::string fileName{dirName + "/cper-XXXXXX"};
    auto fd = mkstemp(fileName.data());
    if (fd < 0)
    {
        lg2::error("Failed to generate temp file, error {ERRORNO}", "ERRORNO",
                   std::strerror(errno));
        return PLDM_ERROR;
    }
    close(fd);

    std::ofstream ofs;
    ofs.exceptions(std::ofstream::failbit | std::ofstream::badbit |
                   std::ofstream::eofbit);

    try
    {
        ofs.open(fileName);
        ofs.write(reinterpret_cast<const char*>(eventData), eventDataSize);
        if (formatType ==
            static_cast<uint8_t>(CperFormatType::PLDM_FORMAT_TYPE_CPER))
        {
            rc = createCperDumpEntry("CPER", fileName);
        }
        else
        {
            rc = createCperDumpEntry("CPERSection", fileName);
        }
        ofs.close();
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to save CPER to '{FILENAME}', error - {ERROR}.",
                   "FILENAME", fileName, "ERROR", e);
        return PLDM_ERROR;
    }
    return rc;
}

int EventManager::createCperDumpEntry(const std::string& dataType,
                                      const std::string& dataPath)
{
    auto createDump = [](std::map<std::string, std::string>& addData) {
        static constexpr auto dumpObjPath =
            "/xyz/openbmc_project/dump/faultlog";
        static constexpr auto dumpInterface = "xyz.openbmc_project.Dump.Create";
        auto& bus = pldm::utils::DBusHandler::getBus();

        try
        {
            auto service = pldm::utils::DBusHandler().getService(dumpObjPath,
                                                                 dumpInterface);
            auto method = bus.new_method_call(service.c_str(), dumpObjPath,
                                              dumpInterface, "CreateDump");
            method.append(addData);
            bus.call_noreply(method);
        }
        catch (const std::exception& e)
        {
            lg2::error("Failed to create D-Bus Dump entry, error - {ERROR}.",
                       "ERROR", e);
        }
    };

    std::map<std::string, std::string> addData;
    addData["CPER_TYPE"] = dataType;
    addData["CPER_PATH"] = dataPath;
    createDump(addData);
    return PLDM_SUCCESS;
}

int EventManager::createSensorThresholdLogEntry(const std::string& messageId,
                                                const std::string& sensorName,
                                                const double reading,
                                                const double threshold)
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
            auto service = pldm::utils::DBusHandler().getService(logObjPath,
                                                                 logInterface);
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

    addData["REDFISH_MESSAGE_ARGS"] = sensorName + "," +
                                      std::to_string(reading) + "," +
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
    auto rc = decode_numeric_sensor_data(sensorData, sensorDataLength,
                                         &eventState, &previousEventState,
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

std::string
    EventManager::getSensorThresholdMessageId(uint8_t previousEventState,
                                              uint8_t eventState)
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
