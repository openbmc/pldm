#include "oem_ibm_handler.hpp"

#include "libpldm/requester/pldm.h"

#include "file_io_type_lid.hpp"
#include "libpldmresponder/file_io.hpp"
#include "libpldmresponder/pdr_utils.hpp"

#include <thread>

namespace pldm
{

namespace responder
{

namespace oem_ibm_platform
{

int pldm::responder::oem_ibm_platform::Handler::
    getOemStateSensorReadingsHandler(
        uint16_t entityType, uint16_t entityInstance, uint16_t stateSetId,
        uint8_t compSensorCnt, std::vector<get_sensor_state_field>& stateField)
{
    int rc = PLDM_SUCCESS;
    stateField.clear();

    for (size_t i = 0; i < compSensorCnt; i++)
    {
        uint8_t sensorOpState{};
        if (entityType == PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE &&
            stateSetId == PLDM_OEM_IBM_BOOT_STATE)
        {
            sensorOpState = fetchBootSide(entityInstance, codeUpdate);
        }
        else
        {
            rc = PLDM_PLATFORM_INVALID_STATE_VALUE;
            break;
        }
        stateField.push_back({PLDM_SENSOR_ENABLED, PLDM_SENSOR_UNKNOWN,
                              PLDM_SENSOR_UNKNOWN, sensorOpState});
    }
    return rc;
}

int pldm::responder::oem_ibm_platform::Handler::
    OemSetStateEffecterStatesHandler(
        uint16_t entityType, uint16_t entityInstance, uint16_t stateSetId,
        uint8_t compEffecterCnt,
        std::vector<set_effecter_state_field>& stateField,
        uint16_t /*effecterId*/)
{
    int rc = PLDM_SUCCESS;

    for (uint8_t currState = 0; currState < compEffecterCnt; ++currState)
    {
        if (stateField[currState].set_request == PLDM_REQUEST_SET)
        {
            if (entityType == PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE &&
                stateSetId == PLDM_OEM_IBM_BOOT_STATE)
            {
                std::cout << "received setBootSide request \n";
                rc = setBootSide(entityInstance, currState, stateField,
                                 codeUpdate);
            }
            else if (entityType == PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE &&
                     stateSetId == 32768)
            {
                if (stateField[currState].effecter_state == START)
                {
                    std::cout << "received start update \n";
                    codeUpdate->setCodeUpdateProgress(true);
                    rc = codeUpdate->setRequestedApplyTime();
                    std::cout << "after setRequestedApplyTime \n";
                    // sendCodeUpdateEvent(effecterId, START, END);
                    auto sensorId = codeUpdate->getFirmwareUpdateSensor();
                    sendStateSensorEvent(sensorId, PLDM_STATE_SENSOR_STATE, 0,
                                         START, END);
                    std::cout << "after sendCodeUpdateEvent returned \n";
                }
                else if (stateField[currState].effecter_state == END)
                {
                    std::cout << "received endupdate \n";
                    rc = PLDM_SUCCESS;
                    assembleImageEvent = std::make_unique<
                        sdeventplus::source::Defer>(
                        event,
                        std::bind(
                            std::mem_fn(
                                &oem_ibm_platform::Handler::_processEndUpdate),
                            this, std::placeholders::_1));
                    /*std::cout << "before assembleCodeUpdateImage \n";
                    int retc = assembleCodeUpdateImage();
                    std::cout << "after assembleCodeUpdateImage \n";
                    if (retc == PLDM_SUCCESS)
                    {
                        std::cout << "assembleCodeUpdateImage returned success
                    \n"; rc = codeUpdate->setRequestedActivation(codeUpdate);
                    }
                    else
                    {
                        std::cerr << "Image assembly Failed ERROR:" << retc
                                  << "\n";
                    }
                    codeUpdate->setCodeUpdateProgress(false);
                    auto sensorId = codeUpdate->getFirmwareUpdateSensor();
                    sendStateSensorEvent(sensorId,PLDM_STATE_SENSOR_STATE,
                    0,END, START);*/
                    //    sendCodeUpdateEvent(effecterId, END, START);
                }
                else if (stateField[currState].effecter_state == ABORT)
                {
                    std::cout << "received ABORT update \n";
                    codeUpdate->setCodeUpdateProgress(false);
                    /*std::unique_ptr<oem_platform::Handler>
                    oemPlatformHandler{}; oem_ibm::Handler
                    handler(oemPlatformHandler.get()); rc =
                    handler.clearDirPath(LID_STAGING_DIR);*/
                    // rc = codeUpdate->clearLids(platformHandler);
                    // sendCodeUpdateEvent(effecterId, ABORT, END);
                    auto sensorId = codeUpdate->getFirmwareUpdateSensor();
                    sendStateSensorEvent(sensorId, PLDM_STATE_SENSOR_STATE, 0,
                                         ABORT, START);
                }
                else if (stateField[currState].effecter_state == ACCEPT)
                {
                    // TODO Set new Dbus property provided by code update app
                    // sendCodeUpdateEvent(effecterId, ACCEPT, END);
                    auto sensorId = codeUpdate->getFirmwareUpdateSensor();
                    sendStateSensorEvent(sensorId, PLDM_STATE_SENSOR_STATE, 0,
                                         ACCEPT, END);
                }
                else if (stateField[currState].effecter_state == REJECT)
                {
                    // TODO Set new Dbus property provided by code update app
                    // sendCodeUpdateEvent(effecterId, REJECT, END);
                    auto sensorId = codeUpdate->getFirmwareUpdateSensor();
                    sendStateSensorEvent(sensorId, PLDM_STATE_SENSOR_STATE, 0,
                                         REJECT, END);
                }
            }
            else
            {
                rc = PLDM_PLATFORM_SET_EFFECTER_UNSUPPORTED_SENSORSTATE;
            }
        }
        if (rc != PLDM_SUCCESS)
        {
            break;
        }
    }
    return rc;
}

void pldm::responder::oem_ibm_platform::Handler::buildOEMPDR(
    pdr_utils::Repo& repo)
{
    buildAllCodeUpdateEffecterPDR(platformHandler, repo);

    buildAllCodeUpdateSensorPDR(platformHandler, repo);

    auto sensorId = findStateSensorId(
        repo.getPdr(), 0, PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE,
        ENTITY_INSTANCE_0, 0, PLDM_OEM_IBM_VERIFICATION_STATE);
    codeUpdate->setMarkerLidSensor(sensorId);
    sensorId = findStateSensorId(
        repo.getPdr(), 0, PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE,
        ENTITY_INSTANCE_0, 0, PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE);
    std::cout << "got sensor id for firmware update " << sensorId << "\n";
    codeUpdate->setFirmwareUpdateSensor(sensorId);
}

void pldm::responder::oem_ibm_platform::Handler::setPlatformHandler(
    pldm::responder::platform::Handler* handler)
{
    platformHandler = handler;
}

int pldm::responder::oem_ibm_platform::Handler::sendEventToHost(
    std::vector<uint8_t>& requestMsg)
{
    uint8_t* responseMsg = nullptr;
    size_t responseMsgSize{};

    std::cout << "sendEventToHost \n\n";

    if (requestMsg.size())
    {
        std::ostringstream tempStream;
        for (int byte : requestMsg)
        {
            tempStream << std::setfill('0') << std::setw(2) << std::hex << byte
                       << " ";
        }
        std::cout << tempStream.str() << std::endl;
    }

    auto requesterRc =
        pldm_send_recv(mctp_eid, mctp_fd, requestMsg.data(), requestMsg.size(),
                       &responseMsg, &responseMsgSize);
    std::unique_ptr<uint8_t, decltype(std::free)*> responseMsgPtr{responseMsg,
                                                                  std::free};
    if (requesterRc != PLDM_REQUESTER_SUCCESS)
    {
        std::cerr << "Failed to send message/receive response. RC = "
                  << requesterRc << ", errno = " << errno
                  << "for sending event to host \n";
        return requesterRc;
    }
    uint8_t completionCode{};
    uint8_t status{};
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsgPtr.get());
    auto rc = decode_platform_event_message_resp(
        responsePtr, responseMsgSize - sizeof(pldm_msg_hdr), &completionCode,
        &status);

    if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
    {
        std::cerr << "Failure in decode platform event message response, rc= "
                  << rc << " cc=" << static_cast<unsigned>(completionCode)
                  << "\n";
        return rc;
    }
    std::cout << "returning rc= " << rc << " from sendEventToHost \n";

    return rc;
}

void pldm::responder::oem_ibm_platform::Handler::sendCodeUpdateEvent(
    uint16_t effecterId, codeUpdateStateValues opState,
    codeUpdateStateValues previousOpState)
{
    std::vector<uint8_t> effecterEventDataVec{};
    size_t effecterEventSize = PLDM_EFFECTER_EVENT_DATA_MIN_LENGTH + 1;
    effecterEventDataVec.resize(effecterEventSize);

    auto eventData = reinterpret_cast<struct pldm_effecter_event_data*>(
        effecterEventDataVec.data());
    eventData->effecter_id = effecterId;
    eventData->effecter_event_class_type = PLDM_EFFECTER_OP_STATE;
    auto eventClassStart = eventData->event_class;

    auto opStateEffecterEventData =
        reinterpret_cast<struct pldm_effecter_event_effecter_op_state*>(
            eventClassStart);
    // effecterEventDataVec.data());
    opStateEffecterEventData->present_op_state = opState;
    opStateEffecterEventData->previous_op_state = previousOpState;

    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                    PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES +
                                    effecterEventDataVec.size());

    auto instanceId = requester.getInstanceId(mctp_eid);

    auto rc = encodeEventMsg(PLDM_EFFECTER_EVENT, effecterEventDataVec,
                             requestMsg, instanceId);

    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Failed to encode state effecter event, rc = " << rc
                  << std::endl;
        return;
    }

    rc = sendEventToHost(requestMsg);

    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Failed to send event to host: "
                  << "rc=" << rc << std::endl;
    }

    requester.markFree(mctp_eid, instanceId);

    return;
}

int encodeEventMsg(uint8_t eventType, const std::vector<uint8_t>& eventDataVec,
                   std::vector<uint8_t>& requestMsg, uint8_t instanceId)
{
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = encode_platform_event_message_req(
        instanceId, 1 /*formatVersion*/, 0 /*tId*/, eventType,
        eventDataVec.data(), eventDataVec.size(), request,
        eventDataVec.size() + PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES);

    return rc;
}

void pldm::responder::oem_ibm_platform::Handler::sendStateSensorEvent(
    uint16_t sensorId, enum sensor_event_class_states sensorEventClass,
    uint8_t sensorOffset, uint8_t eventState, uint8_t prevEventState)
{
    std::cout << "sendStateSensorEvent with sensorId " << sensorId << "\n";
    std::vector<uint8_t> sensorEventDataVec{};
    size_t sensorEventSize = PLDM_SENSOR_EVENT_DATA_MIN_LENGTH + 1;
    sensorEventDataVec.resize(sensorEventSize);

    auto eventData = reinterpret_cast<struct pldm_sensor_event_data*>(
        sensorEventDataVec.data());
    eventData->sensor_id = sensorId;
    eventData->sensor_event_class_type =
        sensorEventClass; // PLDM_STATE_SENSOR_STATE;
    auto eventClassStart = eventData->event_class;

    auto eventClass =
        reinterpret_cast<struct pldm_sensor_event_state_sensor_state*>(
            eventClassStart);
    eventClass->sensor_offset = sensorOffset;
    eventClass->event_state = eventState;
    eventClass->previous_event_state = prevEventState;

    auto instanceId = requester.getInstanceId(mctp_eid);

    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                    PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES +
                                    sensorEventDataVec.size());
    auto rc = encodeEventMsg(PLDM_SENSOR_EVENT, sensorEventDataVec, requestMsg,
                             instanceId);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Failed to encode state sensor event, rc = " << rc
                  << std::endl;
        return;
    }
    rc = sendEventToHost(requestMsg);

    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Failed to send event to host: "
                  << "rc=" << rc << std::endl;
    }
    requester.markFree(mctp_eid, instanceId);
    return;
}

void pldm::responder::oem_ibm_platform::Handler::_processEndUpdate(
    sdeventplus::source::EventBase& /*source */)
{
    std::cout << "entered processEndUpdate \n";
    assembleImageEvent.reset();
    std::cout << "before assembleCodeUpdateImage \n";
    int retc = assembleCodeUpdateImage();
    std::cout << "after assembleCodeUpdateImage \n";
    /*codeUpdateStateValues state = END;
    if (retc == PLDM_SUCCESS)
    {
        std::cout << "assembleCodeUpdateImage returned success \n";
        size_t count = 20;
        while(count)
        {
            std::cout << "looping and waiting for interface added signal \n";
            count--;
            constexpr auto waitTime = std::chrono::seconds(1);
            std::this_thread::sleep_for(waitTime);
            if(codeUpdate->fetchnewImageId() != "")
            {
                std::cout << "interface is added \n";
                break;
            }
        }
        retc = codeUpdate->setRequestedActivation(codeUpdate);
        if(retc != PLDM_SUCCESS)
        {
            std::cerr << "could not set RequestedActivation \n";
            state = FAIL;
        }
    }
    else
    {
        state = FAIL;
        std::cerr << "Image assembly Failed ERROR:" << retc
                  << "\n";
    }
    codeUpdate->setCodeUpdateProgress(false);
    auto sensorId = codeUpdate->getFirmwareUpdateSensor();
    sendStateSensorEvent(sensorId,PLDM_STATE_SENSOR_STATE, 0,state, START);*/
    if (retc != PLDM_SUCCESS)
    {
        codeUpdate->setCodeUpdateProgress(false);
        auto sensorId = codeUpdate->getFirmwareUpdateSensor();
        sendStateSensorEvent(sensorId, PLDM_STATE_SENSOR_STATE, 0, FAIL, START);
    }
}

} // namespace oem_ibm_platform

} // namespace responder

} // namespace pldm
