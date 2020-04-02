#include "host_pdr_handler.hpp"

#include "dbus_impl_requester.hpp"

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
} // namespace pldm
