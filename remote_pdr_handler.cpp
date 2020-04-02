#include "remote_pdr_handler.hpp"

#include "libpldm/requester/pldm.h"

namespace pldm
{

RemotePDRHandler::RemotePDRHandler(uint8_t eid, uint8_t tid) :
    mctp_eid(eid), terminus_id(tid)
{
}

uint8_t completionCode{};
uint32_t nextRecordHandle{};
uint32_t nextDataTransferHandle{};
uint8_t transferFlag{};
uint16_t respCount{};
uint8_t transferCRC{};

std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) + PLDM_GET_PDR_REQ_BYTES);
std::vector<uint8_t> responseMsg;
auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

void RemotePDRHandler::fetchPDR(const std::vector<uint32_t>& recordHandles)
{
    int fd = pldm_open();
    if (-1 == fd)
    {
        std::cerr << "failed to init mctp "
                  << "\n";
        return;
    }

    for (auto recordHandle : recordHandles)
    {
        auto rc = encode_get_pdr_req(request->hdr.instance_id, recordHandle, 0,
                                     PLDM_GET_FIRSTPART, UINT16_MAX, 0, request,
                                     PLDM_GET_PDR_REQ_BYTES);

        uint8_t* responseMessage = nullptr;
        size_t responseMessageSize{};
        pldm_send_recv(mctp_eid, fd, requestMsg.data(), requestMsg.size(),
                       &responseMessage, &responseMessageSize);

        responseMsg.resize(responseMessageSize);
        memcpy(responseMsg.data(), responseMessage, responseMsg.size());
        free(responseMessage);
        auto responsePtr =
            reinterpret_cast<struct pldm_msg*>(responseMsg.data());

        std::vector<uint8_t> pdr;
        rc = decode_get_pdr_resp(
            responsePtr, responseMsg.size() - sizeof(pldm_msg_hdr),
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
