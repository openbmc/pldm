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

    /* EventClass CPEREvent as `Table 11 - PLDM Event Types` DSP0248 V1.3.0 */
    if (eventClass == PLDM_CPER_EVENT)
    {
        return processCperEvent(tid, eventId, eventData, eventDataSize);
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

int EventManager::processCperEvent(pldm_tid_t tid, uint16_t eventId,
                                   const uint8_t* eventData,
                                   const size_t eventDataSize)
{
    if (eventDataSize < PLDM_PLATFORM_CPER_EVENT_MIN_LENGTH)
    {
        lg2::error(
            "Error : Invalid CPER Event data length for eventId {EVENTID}.",
            "EVENTID", eventId);
        return PLDM_ERROR;
    }
    const size_t cperEventDataSize =
        eventDataSize - PLDM_PLATFORM_CPER_EVENT_MIN_LENGTH;
    const size_t msgDataLen =
        sizeof(pldm_platform_cper_event) + cperEventDataSize;
    std::string terminusName = "";
    auto msgData = std::make_unique<unsigned char[]>(msgDataLen);
    auto cperEvent = new (msgData.get()) pldm_platform_cper_event;

    auto rc = decode_pldm_platform_cper_event(eventData, eventDataSize,
                                              cperEvent, msgDataLen);

    if (rc)
    {
        lg2::error(
            "Failed to decode CPER event for eventId {EVENTID} of terminus ID {TID} error {RC}.",
            "EVENTID", eventId, "TID", tid, "RC", rc);
        return rc;
    }

    if (termini.contains(tid) && !termini[tid])
    {
        terminusName = termini[tid]->getTerminusName();
    }

    /* Continue log CPER for terminus does not have Name */
    if (terminusName.empty())
    {
        lg2::error(
            "Source terminus ID {TID} of CPER event for eventId {EVENTID} does not have terminus name.",
            "TID", tid, "EVENTID", eventId);
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
        try
        {
            fs::create_directory(dirName);
        }
        catch (const std::exception& e)
        {
            lg2::error(
                "Failed to create /var/cper directory to store CPER Fault log");
            return PLDM_ERROR;
        }
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
        ofs.write(reinterpret_cast<const char*>(
                      pldm_platform_cper_event_event_data(cperEvent)),
                  cperEvent->event_data_length);
        if (cperEvent->format_type == PLDM_PLATFORM_CPER_EVENT_WITH_HEADER)
        {
            rc = createCperDumpEntry("CPER", fileName, terminusName);
        }
        else
        {
            rc = createCperDumpEntry("CPERSection", fileName, terminusName);
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
                                      const std::string& dataPath,
                                      const std::string& terminusName)
{
    auto createDump =
        [](std::map<std::string, std::variant<std::string, uint64_t>>&
               addData) {
            static constexpr auto dumpObjPath =
                "/xyz/openbmc_project/dump/faultlog";
            static constexpr auto dumpInterface =
                "xyz.openbmc_project.Dump.Create";
            auto& bus = pldm::utils::DBusHandler::getBus();

            try
            {
                auto service = pldm::utils::DBusHandler().getService(
                    dumpObjPath, dumpInterface);
                auto method = bus.new_method_call(service.c_str(), dumpObjPath,
                                                  dumpInterface, "CreateDump");
                method.append(addData);
                bus.call_noreply(method);
            }
            catch (const std::exception& e)
            {
                lg2::error(
                    "Failed to create D-Bus Dump entry, error - {ERROR}.",
                    "ERROR", e);
            }
        };

    std::map<std::string, std::variant<std::string, uint64_t>> addData;
    addData["Type"] = dataType;
    addData["PrimaryLogId"] = dataPath;
    addData["AdditionalTypeName"] = terminusName;
    createDump(addData);
    return PLDM_SUCCESS;
}

} // namespace platform_mc
} // namespace pldm
