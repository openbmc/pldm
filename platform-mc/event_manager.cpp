#include "event_manager.hpp"

#include "libpldm/platform.h"
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

    /* EventClass pldmMessagePollEvent `Table 11 - PLDM Event Types` DSP0248 */
    if (eventClass == PLDM_MESSAGE_POLL_EVENT)
    {
        lg2::info("Received pldmMessagePollEvent for terminus {TID}", "TID",
                  tid);
        struct pldm_message_poll_event poll_event = {};
        auto rc = decode_pldm_message_poll_event_data(eventData, eventDataSize,
                                                      &poll_event);
        if (rc)
        {
            lg2::error(
                "Failed to decode PldmMessagePollEvent event, error {RC} ",
                "RC", rc);
            return rc;
        }

        if (termini.contains(tid))
        {
            termini[tid]->pollEvent = true;
            termini[tid]->pollEventId = poll_event.event_id;
            termini[tid]->pollDataTransferHandle =
                poll_event.data_transfer_handle;
        }
        return PLDM_SUCCESS;
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
        auto tmp = termini[tid]->getTerminusName();
        if (tmp && !tmp.value().empty())
        {
            terminusName = static_cast<std::string>(tmp.value());
        }
    }

    // Save event data to file
    std::filesystem::path dirName{"/var/cper"};
    if (!std::filesystem::exists(dirName))
    {
        try
        {
            std::filesystem::create_directory(dirName);
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            lg2::error("Failed to create /var/cper directory: {ERROR}", "ERROR",
                       e);
            return PLDM_ERROR;
        }
    }

    std::string fileName{dirName.string() + "/cper-XXXXXX"};
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
    catch (const std::ofstream::failure& e)
    {
        lg2::error("Failed to save CPER to '{FILENAME}', error - {ERROR}.",
                   "FILENAME", fileName, "ERROR", e);
        return PLDM_ERROR;
    }
    return rc;
}

int EventManager::createCperDumpEntry(const std::string& dataType,
                                      const std::string& dataPath,
                                      const std::string& typeName)
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
    addData["AdditionalTypeName"] = typeName;
    createDump(addData);
    return PLDM_SUCCESS;
}

exec::task<int> EventManager::pollForPlatformEventTask(
    pldm_tid_t tid, uint16_t /* pollEventId */, uint32_t pollDataTransferHandle)
{
    uint8_t rc = 0;
    uint8_t transferOperationFlag = PLDM_GET_FIRSTPART;
    uint32_t dataTransferHandle = pollDataTransferHandle;
    uint32_t eventIdToAcknowledge = PLDM_PLATFORM_EVENT_ID_NULL;

    uint8_t completionCode;
    uint8_t eventTid;
    uint16_t eventId = 0xffff;
    uint8_t formatVersion = 0x1;
    uint32_t nextDataTransferHandle;
    uint8_t transferFlag;
    uint8_t eventClass;
    uint32_t eventDataSize;
    uint8_t* eventData;
    uint32_t eventDataIntegrityChecksum;

    std::vector<uint8_t> eventMessage{};
    /* reset force stop */
    updateAvailableState(tid, true);

    while (eventId != PLDM_PLATFORM_EVENT_ID_NONE)
    {
        completionCode = 0;
        eventTid = 0;
        eventId = PLDM_PLATFORM_EVENT_ID_NONE;
        nextDataTransferHandle = 0;
        transferFlag = 0;
        eventClass = 0;
        eventDataSize = 0;
        eventData = nullptr;
        eventDataIntegrityChecksum = 0;

        /* Stop event polling */
        if (!getAvailableState(tid))
        {
            lg2::info(
                "Terminus ID {TID} is not available for PLDM request from {NOW}.",
                "TID", tid, "NOW", pldm::utils::getCurrentSystemTime());
            co_await stdexec::just_stopped();
        }

        rc = co_await pollForPlatformEventMessage(
            tid, formatVersion, transferOperationFlag, dataTransferHandle,
            eventIdToAcknowledge, completionCode, eventTid, eventId,
            nextDataTransferHandle, transferFlag, eventClass, eventDataSize,
            eventData, eventDataIntegrityChecksum);
        if (rc || completionCode != PLDM_SUCCESS)
        {
            lg2::error(
                "Failed to pollForPlatformEventMessage for terminus {TID}, event {EVENTID}, error {RC}, complete code {CC}",
                "TID", tid, "EVENTID", eventId, "RC", rc, "CC", completionCode);
            co_return rc;
        }

        if (eventDataSize > 0)
        {
            eventMessage.insert(eventMessage.end(), eventData,
                                eventData + eventDataSize);
        }

        if (transferOperationFlag == PLDM_ACKNOWLEDGEMENT_ONLY)
        {
            if (eventId == PLDM_PLATFORM_EVENT_ID_ACK)
            {
                transferOperationFlag = PLDM_GET_FIRSTPART;
                dataTransferHandle = 0;
                eventIdToAcknowledge = PLDM_PLATFORM_EVENT_ID_NULL;
                eventMessage.clear();
            }
        }
        else
        {
            if (transferFlag != PLDM_PLATFORM_TRANSFER_START_AND_END &&
                transferFlag != PLDM_PLATFORM_TRANSFER_END)
            {
                transferOperationFlag = PLDM_GET_NEXTPART;
                dataTransferHandle = nextDataTransferHandle;
                eventIdToAcknowledge = PLDM_PLATFORM_EVENT_ID_FRAGMENT;
            }
            else
            {
                if (transferFlag == PLDM_PLATFORM_TRANSFER_START_AND_END)
                {
                    if (eventHandlers.contains(eventClass))
                    {
                        eventHandlers.at(
                            eventClass)(eventTid, eventId, eventMessage.data(),
                                        eventMessage.size());
                    }
                }
                else if (transferFlag == PLDM_PLATFORM_TRANSFER_END)
                {
                    if (eventDataIntegrityChecksum ==
                        crc32(eventMessage.data(), eventMessage.size()))
                    {
                        if (eventHandlers.contains(eventClass))
                        {
                            eventHandlers.at(eventClass)(eventTid, eventId,
                                                         eventMessage.data(),
                                                         eventMessage.size());
                        }
                    }
                    else
                    {
                        lg2::error(
                            "pollForPlatformEventMessage for terminus {TID} with event {EVENTID} checksum error.",
                            "TID", tid, "EVENTID", eventId);
                        co_return PLDM_ERROR_INVALID_DATA;
                    }
                }

                transferOperationFlag = PLDM_ACKNOWLEDGEMENT_ONLY;
                dataTransferHandle = 0;
                eventIdToAcknowledge = eventId;
            }
        }
    }

    co_return PLDM_SUCCESS;
}

exec::task<int> EventManager::pollForPlatformEventMessage(
    pldm_tid_t tid, uint8_t formatVersion, uint8_t transferOperationFlag,
    uint32_t dataTransferHandle, uint16_t eventIdToAcknowledge,
    uint8_t& completionCode, uint8_t& eventTid, uint16_t& eventId,
    uint32_t& nextDataTransferHandle, uint8_t& transferFlag,
    uint8_t& eventClass, uint32_t& eventDataSize, uint8_t*& eventData,
    uint32_t& eventDataIntegrityChecksum)
{
    Request request(
        sizeof(pldm_msg_hdr) + PLDM_POLL_FOR_PLATFORM_EVENT_MESSAGE_REQ_BYTES);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
    auto rc = encode_poll_for_platform_event_message_req(
        0, formatVersion, transferOperationFlag, dataTransferHandle,
        eventIdToAcknowledge, requestMsg, request.size());
    if (rc)
    {
        lg2::error(
            "Failed to encode request PollForPlatformEventMessage for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    /* Stop event polling */
    if (!getAvailableState(tid))
    {
        lg2::info(
            "Terminus ID {TID} is not available for PLDM request from {NOW}.",
            "TID", tid, "NOW", pldm::utils::getCurrentSystemTime());
        co_await stdexec::just_stopped();
    }

    const pldm_msg* responseMsg = nullptr;
    size_t responseLen = 0;
    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        lg2::error(
            "Failed to send PollForPlatformEventMessage message for terminus {TID}, error {RC}",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    rc = decode_poll_for_platform_event_message_resp(
        responseMsg, responseLen, &completionCode, &eventTid, &eventId,
        &nextDataTransferHandle, &transferFlag, &eventClass, &eventDataSize,
        (void**)&eventData, &eventDataIntegrityChecksum);
    if (rc)
    {
        lg2::error(
            "Failed to decode response PollForPlatformEventMessage for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return rc;
    }
    if (completionCode != PLDM_SUCCESS)
    {
        lg2::error(
            "Error : PollForPlatformEventMessage for terminus ID {TID}, complete code {CC}.",
            "TID", tid, "CC", completionCode);
        co_return rc;
    }

    co_return completionCode;
}

} // namespace platform_mc
} // namespace pldm
