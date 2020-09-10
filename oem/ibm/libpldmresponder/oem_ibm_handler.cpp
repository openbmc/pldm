#include "oem_ibm_handler.hpp"

#include "libpldm/requester/pldm.h"

#include "libpldmresponder/pdr_utils.hpp"

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
        if (entityType == PLDM_VIRTUAL_MACHINE_MANAGER_ENTITY &&
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
        const std::vector<set_effecter_state_field>& stateField)
{
    int rc = PLDM_SUCCESS;

    for (uint8_t currState = 0; currState < compEffecterCnt; ++currState)
    {
        if (stateField[currState].set_request == PLDM_REQUEST_SET)
        {
            if (entityType == PLDM_VIRTUAL_MACHINE_MANAGER_ENTITY &&
                stateSetId == PLDM_OEM_IBM_BOOT_STATE)
            {
                rc = setBootSide(entityInstance, currState, stateField,
                                 codeUpdate);
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
    pdr_utils::RepoInterface& repo)
{
    buildAllCodeUpdateEffecterPDR(platformHandler, repo);

    buildAllCodeUpdateSensorPDR(platformHandler, repo);
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

    auto requesterRc =
        pldm_send_recv(mctp_eid, mctp_fd, requestMsg.data(), requestMsg.size(),
                       &responseMsg, &responseMsgSize);
    std::unique_ptr<uint8_t, decltype(std::free)*> responseMsgPtr{responseMsg,
                                                                  std::free};
    if (requesterRc != PLDM_REQUESTER_SUCCESS)
    {
        return requesterRc;
    }
    uint8_t completionCode{};
    uint8_t status{};
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsgPtr.get());
    auto rc = decode_platform_event_message_resp(
        responsePtr, responseMsgSize - sizeof(pldm_msg_hdr), &completionCode,
        &status);

    if (completionCode != PLDM_SUCCESS)
    {
        return PLDM_ERROR;
    }

    return rc;
}

void pldm::responder::oem_ibm_platform::Handler::sendCodeUpdateEvent(
    EffecterId effecterId, codeUpdateStateValues opState,
    codeUpdateStateValues previousOpState)
{
    std::vector<uint8_t> effecterEventDataVec{};
    size_t effecterEventSize = PLDM_EFFECTER_EVENT_DATA_MIN_LENGTH + 1;
    effecterEventDataVec.resize(effecterEventSize);

    auto eventData = reinterpret_cast<struct pldm_effecter_event_data*>(
        effecterEventDataVec.data());
    eventData->effecter_id = effecterId;
    eventData->effecter_event_class_type = PLDM_EFFECTER_OP_STATE;

    auto opStateEffecterEventData =
        reinterpret_cast<struct pldm_effecter_event_effecter_op_state*>(
            effecterEventDataVec.data());
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
        instanceId, 1 /*formatVersion*/, 0 /*tId*/, eventType,
        eventDataVec.data(), eventDataVec.size(), request,
        eventDataVec.size() + PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES);

    return rc;
}

} // namespace oem_ibm_platform

} // namespace responder

} // namespace pldm
