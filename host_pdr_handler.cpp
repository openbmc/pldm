#include "host_pdr_handler.hpp"

#include "libpldm/requester/pldm.h"

namespace pldm
{

void HostPDRHandler::fetchPDR(std::vector<uint32_t>&& recordHandles)
{
    pdrRecordHandles.clear();
    pdrRecordHandles = std::move(recordHandles);

    // Defer the actual fetch of PDRs from the host (by queuing the call on the
    // main event loop). That way, we can respond to the platform event msg from
    // the host firmware.
    pdrFetchEvent = std::make_unique<sdeventplus::source::Defer>(
        event, std::bind(std::mem_fn(&HostPDRHandler::_fetchPDR), this,
                         std::placeholders::_1));
}

void HostPDRHandler::_fetchPDR(sdeventplus::source::EventBase& /*source*/)
{
    pdrFetchEvent.reset();

    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                    PLDM_GET_PDR_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

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

        uint8_t* responseMsg = nullptr;
        size_t responseMsgSize{};
        auto requesterRc =
            pldm_send_recv(mctp_eid, mctp_fd, requestMsg.data(),
                           requestMsg.size(), &responseMsg, &responseMsgSize);
        std::unique_ptr<uint8_t, decltype(std::free)*> responseMsgPtr{
            responseMsg, std::free};
        requester.markFree(mctp_eid, instanceId);
        if (requesterRc != PLDM_REQUESTER_SUCCESS)
        {
            std::cerr << "Failed to send msg to fetch pdrs, rc = "
                      << requesterRc << std::endl;
            return;
        }

        auto responsePtr =
            reinterpret_cast<struct pldm_msg*>(responseMsgPtr.get());
        uint8_t completionCode{};
        uint32_t nextRecordHandle{};
        uint32_t nextDataTransferHandle{};
        uint8_t transferFlag{};
        uint16_t respCount{};
        uint8_t transferCRC{};
        rc = decode_get_pdr_resp(
            responsePtr, responseMsgSize - sizeof(pldm_msg_hdr),
            &completionCode, &nextRecordHandle, &nextDataTransferHandle,
            &transferFlag, &respCount, nullptr, 0, &transferCRC);
        if (rc != PLDM_SUCCESS)
        {
            std::cerr << "Failed to decode_get_pdr_resp, rc = " << rc
                      << std::endl;
        }
        else
        {
            std::vector<uint8_t> pdr(respCount, 0);
            rc = decode_get_pdr_resp(
                responsePtr, responseMsgSize - sizeof(pldm_msg_hdr),
                &completionCode, &nextRecordHandle, &nextDataTransferHandle,
                &transferFlag, &respCount, pdr.data(), respCount, &transferCRC);
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
