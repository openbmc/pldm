#include "host_pdr_handler.hpp"

#include <assert.h>

#include <vector>

#include "libpldm/platform.h"
#include "libpldm/requester/pldm.h"

namespace pldm
{
void HostPDRHandler::fetchPDR(const std::vector<uint32_t>& recordHandles,
                              uint8_t mctp_eid, int fd, Requester& requester)
{
    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                    PLDM_GET_PDR_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    uint8_t completionCode{};
    uint32_t nextRecordHandle{};
    uint32_t nextDataTransferHandle{};
    uint8_t transferFlag{};
    uint16_t respCount{};
    uint8_t transferCRC{};
    for (auto recordHandle : recordHandles)
    {
        auto rc = encode_get_pdr_req(
            requester.getInstanceId(mctp_eid), recordHandle, 0,
            PLDM_GET_FIRSTPART, UINT16_MAX, 0, request, PLDM_GET_PDR_REQ_BYTES);

        uint8_t* responseMessage = nullptr;
        size_t responseMessageSize{};
        pldm_send_recv(mctp_eid, fd, requestMsg.data(), requestMsg.size(),
                       &responseMessage, &responseMessageSize);

        auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMessage);

        std::vector<uint8_t> pdr;
        rc = decode_get_pdr_resp(
            responsePtr, responseMessageSize - sizeof(pldm_msg_hdr),
            &completionCode, &nextRecordHandle, &nextDataTransferHandle,
            &transferFlag, &respCount, 0, 0, &transferCRC);

        pdr.resize(respCount);

        rc = decode_get_pdr_resp(
            responsePtr, responseMessageSize - sizeof(pldm_msg_hdr),
            &completionCode, &nextRecordHandle, &nextDataTransferHandle,
            &transferFlag, &respCount, pdr.data(), pdr.size(), &transferCRC);

        pdrs.emplace_back(std::move(pdr));

        if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc
                      << ",cc=" << static_cast<int>(completionCode)
                      << std::endl;
            std::cout << "Record Handle" << recordHandle << std::endl;
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
} // namespace pldm
