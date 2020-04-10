#include "host_pdr_handler.hpp"

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

} // namespace pldm
