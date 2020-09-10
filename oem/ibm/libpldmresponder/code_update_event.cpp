#include "code_update_event.hpp"

#include "libpldm/requester/pldm.h"

#include <iostream>

namespace pldm
{

using mctpEid = uint8_t;
mctpEid mctpEID = 0;

namespace code_update_event
{

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

int sendEventToHost(std::vector<uint8_t>& requestMsg, uint8_t mctp_fd)
{
    uint8_t* responseMsg = nullptr;
    size_t responseMsgSize{};

    auto requesterRc =
        pldm_send_recv(mctpEID, mctp_fd, requestMsg.data(), requestMsg.size(),
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

    return rc;
}

void sendStateSensorEvent(uint8_t mctp_eid, uint8_t instanceId, uint8_t mctp_fd,
                          SensorId sensorId, codeUpdateStateValues eventState,
                          codeUpdateStateValues previousEventState,
                          uint8_t stateSenserEventPtr)
{
    std::vector<uint8_t> sensorEventDataVec{};
    size_t sensorEventSize = PLDM_SENSOR_EVENT_DATA_MIN_LENGTH + 1;
    sensorEventDataVec.resize(sensorEventSize);

    mctpEID = mctp_eid;

    auto eventData = reinterpret_cast<struct pldm_sensor_event_data*>(
        sensorEventDataVec.data());
    eventData->sensor_id = sensorId;
    eventData->sensor_event_class_type = PLDM_STATE_SENSOR_STATE;

    auto stateSensorEventData =
        reinterpret_cast<struct pldm_sensor_event_state_sensor_state*>(
            sensorEventDataVec.data());
    stateSensorEventData->sensor_offset = stateSenserEventPtr;
    stateSensorEventData->event_state = eventState;
    stateSensorEventData->previous_event_state = previousEventState;

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

    rc = sendEventToHost(requestMsg, mctp_fd);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Failed to send event to host: "
                  << "rc=" << rc << std::endl;
    }
    return;
}

} // namespace code_update_event
} // namespace pldm
