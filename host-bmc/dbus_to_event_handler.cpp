#include "dbus_to_event_handler.hpp"

#include "libpldm/requester/pldm.h"

#include "libpldmresponder/pdr.hpp"

namespace pldm
{

using namespace pldm::responder::pdr;
using namespace pldm::utils;
using namespace sdbusplus::bus::match::rules;

const std::vector pdrTypes{PLDM_STATE_SENSOR_PDR};

DbusToSensorEventHandler::DbusToSensorEventHandler(int mctp_fd,
                                                   uint8_t mctp_eid,
                                                   Requester& requester) :
    mctp_fd(mctp_fd),
    mctp_eid(mctp_eid), requester(requester)
{}

void DbusToSensorEventHandler::sendEventMsg(const uint8_t eventType,
                                            const uint8_t* eventData,
                                            size_t eventSize)
{
    auto instanceId = requester.getInstanceId(mctp_eid);
    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                    PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES +
                                    eventSize);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_platform_event_message_req(
        instanceId, 1, 0, eventType, eventData, eventSize, request,
        eventSize + PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES);
    if (rc != PLDM_SUCCESS)
    {
        requester.markFree(mctp_eid, instanceId);
        std::cerr << "Failed to encode_platform_event_message_req, rc = " << rc
                  << std::endl;
        return;
    }

    uint8_t* responseMsg = nullptr;
    size_t responseMsgSize{};
    auto requesterRc =
        pldm_send_recv(mctp_eid, mctp_fd, requestMsg.data(), requestMsg.size(),
                       &responseMsg, &responseMsgSize);
    requester.markFree(mctp_eid, instanceId);
    if (requesterRc != PLDM_REQUESTER_SUCCESS)
    {
        std::cerr << "Failed to send msg to report pdrs, rc = " << requesterRc
                  << std::endl;
        return;
    }
    uint8_t completionCode{};
    uint8_t status{};
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg);
    rc = decode_platform_event_message_resp(
        responsePtr, responseMsgSize - sizeof(pldm_msg_hdr), &completionCode,
        &status);
    free(responseMsg);
    if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
    {
        std::cerr << "Failed to decode_platform_event_message_resp: "
                  << "rc=" << rc
                  << ", cc=" << static_cast<unsigned>(completionCode)
                  << std::endl;
    }
}

void DbusToSensorEventHandler::sendStateSensorEvent(const uint8_t sensorId)
{
    // Encode PLDM platform event msg to indicate a state sensor change.
    size_t sensorEventSize = PLDM_SENSOR_EVENT_DATA_MIN_LENGTH + 1;
    const auto& [dbusMappings, dbusValMaps] = sensorDbusMaps.at(sensorId);
    for (uint8_t offset = 0; offset < dbusMappings.size(); ++offset)
    {
        std::vector<uint8_t> eventDataVec{};
        eventDataVec.resize(sensorEventSize);
        auto eventData = reinterpret_cast<struct pldm_sensor_event_data*>(
            eventDataVec.data());
        eventData->sensor_id = sensorId;
        eventData->sensor_event_class_type = PLDM_STATE_SENSOR_STATE;
        eventData->event_class[0] = offset;
        eventData->event_class[1] = UNKNOWN;
        eventData->event_class[2] = UNKNOWN;

        auto dbusMapping = dbusMappings[offset];
        stateSensorMatch = std::make_unique<sdbusplus::bus::match::match>(
            pldm::utils::DBusHandler::getBus(),
            propertiesChanged(dbusMapping.objectPath.c_str(),
                              dbusMapping.interface.c_str()),
            [this, eventDataVec, sensorEventSize](auto& /*msg*/) {
                this->sendEventMsg(PLDM_SENSOR_EVENT, eventDataVec.data(),
                                   sensorEventSize);
            });
    }
}

void DbusToSensorEventHandler::listenSensorEvent(pdr_utils::Repo repo,
                                                 DbusObjMaps dbusMaps)
{
    sensorDbusMaps = dbusMaps;
    const std::map<Type, sensorEvent> sensorHandlers = {
        {PLDM_STATE_SENSOR_PDR, [this](const uint8_t& sensorId) {
             this->sendStateSensorEvent(sensorId);
         }}};

    pldm_state_sensor_pdr* pdr = nullptr;
    std::unique_ptr<pldm_pdr, decltype(&pldm_pdr_destroy)> sensorPdrRepo(
        pldm_pdr_init(), pldm_pdr_destroy);

    for (auto pdrType : pdrTypes)
    {
        Repo sensorPDRs(sensorPdrRepo.get());
        getRepoByType(repo, sensorPDRs, pdrType);
        if (sensorPDRs.empty())
        {
            std::cerr << "Failed to get record by PDR type\n";
            return;
        }

        PdrEntry pdrEntry{};
        auto pdrRecord = sensorPDRs.getFirstRecord(pdrEntry);
        while (pdrRecord)
        {
            pdr = reinterpret_cast<pldm_state_sensor_pdr*>(pdrEntry.data);
            uint8_t sensorId = pdr->sensor_id;
            sensorHandlers.at(pdrType)(sensorId);
            pdrRecord = sensorPDRs.getNextRecord(pdrRecord, pdrEntry);
        }
    }
}

} // namespace pldm
