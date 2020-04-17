#include "host_pdr_handler.hpp"

#include "libpldmresponder/pdr.hpp"

#include <assert.h>

#include <bitset>
#include <climits>
#include <vector>

#include "libpldm/platform.h"
#include "libpldm/requester/pldm.h"

namespace pldm
{

void HostPDRHandler::fetchPDR(std::vector<uint32_t>&& recordHandles)
{
    pdrRecordHandles.clear();
    pdrRecordHandles = std::move(recordHandles);

    pdrFetcherEventSrc = std::make_unique<sdeventplus::source::Defer>(
        event, std::bind(std::mem_fn(&HostPDRHandler::_fetchPDR), this,
                         std::placeholders::_1));
}

void HostPDRHandler::_fetchPDR(sdeventplus::source::EventBase& /*source*/)
{
    pdrFetcherEventSrc.reset();

    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                    PLDM_GET_PDR_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    uint8_t completionCode{};
    uint32_t nextRecordHandle{};
    uint32_t nextDataTransferHandle{};
    uint8_t transferFlag{};
    uint16_t respCount{};
    uint8_t transferCRC{};

    for (auto recordHandle : pdrRecordHandles)
    {
        auto instanceId = requester.getInstanceId(mctp_eid);

        auto rc =
            encode_get_pdr_req(instanceId, recordHandle, 0, PLDM_GET_FIRSTPART,
                               UINT16_MAX, 0, request, PLDM_GET_PDR_REQ_BYTES);
        if (rc != PLDM_SUCCESS)
        {
            requester.markFree(mctp_eid, instanceId);
            std::cerr << "Failed to encode_get_pdr_req, rc = " << rc
                      << std::endl;
            return;
        }

        uint8_t* responseMessage = nullptr;
        size_t responseMessageSize{};
        for (auto b : requestMsg)
        {
            std::cerr << std::setfill('0') << std::setw(2) << std::hex
                      << (unsigned)b << " ";
        }
        std::cerr << std::endl;
        auto requesterRc = pldm_send_recv(mctp_eid, mctp_fd, requestMsg.data(),
                                          requestMsg.size(), &responseMessage,
                                          &responseMessageSize);
        requester.markFree(mctp_eid, instanceId);
        if (requesterRc != PLDM_REQUESTER_SUCCESS)
        {
            std::cerr << "Failed to send msg to fetch pdrs, rc = "
                      << requesterRc << std::endl;
            return;
        }

        auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMessage);
        std::vector<uint8_t> pdr;
        rc = decode_get_pdr_resp(
            responsePtr, responseMessageSize - sizeof(pldm_msg_hdr),
            &completionCode, &nextRecordHandle, &nextDataTransferHandle,
            &transferFlag, &respCount, nullptr, 0, &transferCRC);
        if (rc != PLDM_SUCCESS)
        {
            std::cerr << "Failed to decode_get_pdr_resp, rc = " << rc
                      << std::endl;
        }
        else
        {
            pdr.resize(respCount);
            rc = decode_get_pdr_resp(
                responsePtr, responseMessageSize - sizeof(pldm_msg_hdr),
                &completionCode, &nextRecordHandle, &nextDataTransferHandle,
                &transferFlag, &respCount, pdr.data(), respCount, &transferCRC);
            free(responseMessage);
            if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
            {
                std::cerr << "Failed to decode_get_pdr_resp: "
                          << "rc=" << rc
                          << ", cc=" << static_cast<unsigned>(completionCode)
                          << std::endl;
            }
            else
            {
                pldm_pdr_add(repo, pdr.data(), respCount, 0);
            }
        }
    }
}
std::vector<uint8_t> HostPDRHandler::preparepldmPDRRepositoryChgEventData(
    const std::vector<uint8_t>& pdrTypes, uint8_t eventDataFormat,
    const pldm_pdr* repo)
{
    assert(eventDataFormat == FORMAT_IS_PDR_HANDLES);
    uint8_t* outData = nullptr;
    uint32_t size{};
    std::vector<uint32_t> recordHandles;
    const uint8_t numberofChangeRecords = 1;
    const uint8_t eventDataOperation = PLDM_RECORDS_ADDED;
    size_t* actual_change_record_size = nullptr;
    for (auto pdrType : pdrTypes)
    {
        auto record = pldm_pdr_find_record_by_type(repo, pdrType, nullptr,
                                                   &outData, &size);
        if (record)
        {
            struct pldm_pdr_hdr* hdr = reinterpret_cast<pldm_pdr_hdr*>(outData);
            recordHandles.push_back(hdr->record_handle);
        }
    }
    const uint8_t numberofChangeEntries = recordHandles.size();

    size_t max_change_record_size =
        PLDM_PDR_REPOSITORY_CHG_EVENT_MIN_LENGTH +
        PLDM_PDR_REPOSITORY_CHANGE_RECORD_MIN_LENGTH * numberofChangeRecords +
        numberofChangeEntries * (sizeof(uint32_t));
    std::vector<uint8_t> eventDataVec(
        PLDM_PDR_REPOSITORY_CHG_EVENT_MIN_LENGTH +
        PLDM_PDR_REPOSITORY_CHANGE_RECORD_MIN_LENGTH * numberofChangeRecords +
        numberofChangeEntries * (sizeof(uint32_t)));
    const uint32_t* recHandl = &recordHandles[0];

    struct pldm_pdr_repository_chg_event_data* eventData =
        reinterpret_cast<struct pldm_pdr_repository_chg_event_data*>(
            eventDataVec.data());

    auto rc = encode_pldm_pdr_repository_chg_event_data(
        eventDataFormat, numberofChangeRecords, &eventDataOperation,
        &numberofChangeEntries, &recHandl, eventData, actual_change_record_size,
        max_change_record_size);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Response Message Error: "
                  << "rc=" << rc << std::endl;
    }
    return eventDataVec;
}

int HostPDRHandler::sendpldmPDRRepositoryChgEventData(
    const std::vector<uint8_t> eventData, uint8_t mctp_eid, int fd,
    Requester& requester)
{
    uint8_t format_version = 0x01;
    uint8_t tid = 1;
    uint8_t event_class = 0x04;
    auto size = eventData.size();
    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr));
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_platform_event_message_req(
        requester.getInstanceId(mctp_eid), format_version, tid, event_class,
        eventData.data(), size, request);
    uint8_t* responseMessage = nullptr;
    size_t responseMessageSize{};
    rc = pldm_send_recv(mctp_eid, fd, requestMsg.data(), requestMsg.size(),
                        &responseMessage, &responseMessageSize);
    free(responseMessage);
    return rc;
}

void HostPDRHandler::parseStateSensorPDRs(pldm_pdr* repo)
{
    std::unique_ptr<pldm_pdr, decltype(&pldm_pdr_destroy)> stateSensorPdrRepo(
        pldm_pdr_init(), pldm_pdr_destroy);

    Repo stateSensorPDRs(stateSensorPdrRepo.get());
    Repo hostPDRRepo(repo);
    responder::pdr::getRepoByType(hostPDRRepo, stateSensorPDRs,
                                  PLDM_STATE_SENSOR_PDR);
    if (stateSensorPDRs.empty())
    {
        return;
    }

    PdrEntry pdrEntry{};
    auto pdrRecord = stateSensorPDRs.getFirstRecord(pdrEntry);
    while (pdrRecord)
    {
        auto pdr = reinterpret_cast<pldm_state_sensor_pdr*>(pdrEntry.data);
        CompositeSensorStates sensors{};
        auto statesPtr = pdr->possible_states;

        while ((pdr->composite_sensor_count)--)
        {
            auto state =
                reinterpret_cast<state_sensor_possible_states*>(statesPtr);
            PossibleStates possibleStates{};
            uint8_t possibleStatesPos = 0;
            auto updateStates = [&possibleStates,
                                 &possibleStatesPos](const bitfield8_t& val) {
                for (int i = 0; i < CHAR_BIT; i++)
                {
                    if (val.byte & (1 << i))
                    {
                        possibleStates.insert(possibleStatesPos * CHAR_BIT + i);
                    }
                }
                possibleStatesPos++;
            };
            std::for_each(&state->states[0],
                          &state->states[state->possible_states_size + 1],
                          updateStates);

            sensors.emplace_back(std::move(possibleStates));
            statesPtr += sizeof(state_sensor_possible_states) +
                         state->possible_states_size - 1;
        }
        // Invalidate statesPtr
        statesPtr = nullptr;

        auto entityInfo =
            std::make_tuple(static_cast<uint16_t>(pdr->container_id),
                            static_cast<uint16_t>(pdr->entity_type),
                            static_cast<uint16_t>(pdr->entity_instance));
        auto sensorInfo =
            std::make_tuple(std::move(entityInfo), std::move(sensors));
        SensorEntry sensorEntry{};
        // TODO: Lookup Terminus ID from the Terminus Locator PDR with
        // pdr->terminus_handle
        sensorEntry.terminusID = 0;
        sensorEntry.sensorID = pdr->sensor_id;
        sensorMap.emplace(sensorEntry, std::move(sensorInfo));

        pdrRecord = stateSensorPDRs.getNextRecord(pdrRecord, pdrEntry);
    }

    return;
}

} // namespace pldm
