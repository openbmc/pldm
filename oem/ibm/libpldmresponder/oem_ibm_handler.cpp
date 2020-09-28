#include "oem_ibm_handler.hpp"

#include "libpldm/entity.h"
#include "libpldm/requester/pldm.h"

#include "file_io_type_lid.hpp"
#include "libpldmresponder/file_io.hpp"
#include "libpldmresponder/pdr_utils.hpp"
namespace pldm
{
namespace responder
{
namespace oem_ibm_platform
{

int pldm::responder::oem_ibm_platform::Handler::
    getOemStateSensorReadingsHandler(
        EntityType entityType, EntityInstance entityInstance,
        StateSetId stateSetId, CompositeCount compSensorCnt,
        std::vector<get_sensor_state_field>& stateField)
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
                rc = setBootSide(entityInstance, currState, stateField,
                                 codeUpdate);
            }
            else if (entityType == PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE &&
                     stateSetId == PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE)
            {
                if (stateField[currState].effecter_state ==
                    uint8_t(CodeUpdateState::START))
                {
                    codeUpdate->setCodeUpdateProgress(true);
                    rc = codeUpdate->setRequestedApplyTime();
                }
                else if (stateField[currState].effecter_state ==
                         uint8_t(CodeUpdateState::END))
                {
                    codeUpdate->setCodeUpdateProgress(false);
                }
                else if (stateField[currState].effecter_state ==
                         uint8_t(CodeUpdateState::ABORT))
                {
                    codeUpdate->setCodeUpdateProgress(false);
                    pldm::responder::oem_ibm::clearDirPath(LID_STAGING_DIR);
                }
                else if (stateField[currState].effecter_state ==
                         uint8_t(CodeUpdateState::ACCEPT))
                {
                    // TODO Set new Dbus property provided by code update app
                }
                else if (stateField[currState].effecter_state ==
                         uint8_t(CodeUpdateState::REJECT))
                {
                    // TODO Set new Dbus property provided by code update app
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

void buildAllCodeUpdateEffecterPDR(platform::Handler* platformHandler,
                                   uint16_t entityInstance, uint16_t stateSetID,
                                   pdr_utils::Repo& repo)
{
    size_t pdrSize = 0;
    pdrSize = sizeof(pldm_state_effecter_pdr) +
              sizeof(state_effecter_possible_states);
    std::vector<uint8_t> entry{};
    entry.resize(pdrSize);
    pldm_state_effecter_pdr* pdr =
        reinterpret_cast<pldm_state_effecter_pdr*>(entry.data());
    if (!pdr)
    {
        std::cerr << "Failed to get record by PDR type, ERROR:"
                  << PLDM_PLATFORM_INVALID_EFFECTER_ID << std::endl;
    }
    pdr->hdr.record_handle = 0;
    pdr->hdr.version = 1;
    pdr->hdr.type = PLDM_STATE_EFFECTER_PDR;
    pdr->hdr.record_change_num = 0;
    pdr->hdr.length = sizeof(pldm_state_effecter_pdr) - sizeof(pldm_pdr_hdr);
    pdr->terminus_handle = pdr::BmcPldmTerminusHandle;
    pdr->effecter_id = platformHandler->getNextEffecterId();
    pdr->entity_type = PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE;
    pdr->entity_instance = entityInstance;
    pdr->container_id = 0;
    pdr->effecter_semantic_id = 0;
    pdr->effecter_init = PLDM_NO_INIT;
    pdr->has_description_pdr = false;
    pdr->composite_effecter_count = 1;

    auto* possibleStatesPtr = pdr->possible_states;
    auto possibleStates =
        reinterpret_cast<state_effecter_possible_states*>(possibleStatesPtr);
    possibleStates->state_set_id = stateSetID;
    possibleStates->possible_states_size = 2;
    auto state =
        reinterpret_cast<state_effecter_possible_states*>(possibleStates);
    if (stateSetID == PLDM_OEM_IBM_BOOT_STATE)
        state->states[0].byte = 6;
    else if (stateSetID == PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE)
        state->states[0].byte = 126;
    pldm::responder::pdr_utils::PdrEntry pdrEntry{};
    pdrEntry.data = entry.data();
    pdrEntry.size = pdrSize;
    repo.addRecord(pdrEntry);
}

void buildAllCodeUpdateSensorPDR(platform::Handler* platformHandler,
                                 uint16_t entityInstance, uint16_t stateSetID,
                                 pdr_utils::Repo& repo)
{
    size_t pdrSize = 0;
    pdrSize =
        sizeof(pldm_state_sensor_pdr) + sizeof(state_sensor_possible_states);
    std::vector<uint8_t> entry{};
    entry.resize(pdrSize);
    pldm_state_sensor_pdr* pdr =
        reinterpret_cast<pldm_state_sensor_pdr*>(entry.data());
    if (!pdr)
    {
        std::cerr << "Failed to get record by PDR type, ERROR:"
                  << PLDM_PLATFORM_INVALID_SENSOR_ID << std::endl;
    }
    pdr->hdr.record_handle = 0;
    pdr->hdr.version = 1;
    pdr->hdr.type = PLDM_STATE_SENSOR_PDR;
    pdr->hdr.record_change_num = 0;
    pdr->hdr.length = sizeof(pldm_state_sensor_pdr) - sizeof(pldm_pdr_hdr);
    pdr->terminus_handle = pdr::BmcPldmTerminusHandle;
    pdr->sensor_id = platformHandler->getNextSensorId();
    pdr->entity_type = PLDM_ENTITY_VIRTUAL_MACHINE_MANAGER;
    pdr->entity_instance = entityInstance;
    pdr->container_id = 0;
    pdr->sensor_init = PLDM_NO_INIT;
    pdr->sensor_auxiliary_names_pdr = false;
    pdr->composite_sensor_count = 1;

    auto* possibleStatesPtr = pdr->possible_states;
    auto possibleStates =
        reinterpret_cast<state_sensor_possible_states*>(possibleStatesPtr);
    possibleStates->state_set_id = stateSetID;
    possibleStates->possible_states_size = 2;
    auto state =
        reinterpret_cast<state_sensor_possible_states*>(possibleStates);
    if (stateSetID == PLDM_OEM_IBM_BOOT_STATE)
        state->states[0].byte = 6;
    else if (stateSetID == PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE)
        state->states[0].byte = 126;
    pldm::responder::pdr_utils::PdrEntry pdrEntry{};
    pdrEntry.data = entry.data();
    pdrEntry.size = pdrSize;
    repo.addRecord(pdrEntry);
}

void pldm::responder::oem_ibm_platform::Handler::buildOEMPDR(
    pdr_utils::Repo& repo)
{
    buildAllCodeUpdateEffecterPDR(platformHandler, ENTITY_INSTANCE_0,
                                  PLDM_OEM_IBM_BOOT_STATE, repo);
    buildAllCodeUpdateEffecterPDR(platformHandler, ENTITY_INSTANCE_1,
                                  PLDM_OEM_IBM_BOOT_STATE, repo);
    buildAllCodeUpdateEffecterPDR(platformHandler, ENTITY_INSTANCE_0,
                                  PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE, repo);

    buildAllCodeUpdateSensorPDR(platformHandler, ENTITY_INSTANCE_0,
                                PLDM_OEM_IBM_BOOT_STATE, repo);
    buildAllCodeUpdateSensorPDR(platformHandler, ENTITY_INSTANCE_1,
                                PLDM_OEM_IBM_BOOT_STATE, repo);
    buildAllCodeUpdateSensorPDR(platformHandler, ENTITY_INSTANCE_0,
                                PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE, repo);
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

void pldm::responder::oem_ibm_platform::Handler::sendStateSensorEvent(
    uint16_t sensorId, enum sensor_event_class_states sensorEventClass,
    uint8_t sensorOffset, uint8_t eventState, uint8_t prevEventState)
{

    std::vector<uint8_t> sensorEventDataVec{};
    size_t sensorEventSize = PLDM_SENSOR_EVENT_DATA_MIN_LENGTH + 1;
    sensorEventDataVec.resize(sensorEventSize);
    auto eventData = reinterpret_cast<struct pldm_sensor_event_data*>(
        sensorEventDataVec.data());
    eventData->sensor_id = sensorId;
    eventData->sensor_event_class_type = sensorEventClass;
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

int encodeEventMsg(uint8_t eventType, const std::vector<uint8_t>& eventDataVec,
                   std::vector<uint8_t>& requestMsg, uint8_t instanceId)
{
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = encode_platform_event_message_req(
        instanceId, 1 /*formatVersion*/, pldm::responder::pdr::BmcTerminusId,
        eventType, eventDataVec.data(), eventDataVec.size(), request,
        eventDataVec.size() + PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES);

    return rc;
}

} // namespace oem_ibm_platform

} // namespace responder

} // namespace pldm
