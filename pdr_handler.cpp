#include "pdr_handler.hpp"

#include "libpldm/requester/pldm.h"

namespace pldm
{

RemotePDRHandler::RemotePDRHandler(uint8_t eid, uint8_t tid)
{
    mctp_eid = eid;
    terminus_id = tid;
}
static constexpr uint16_t requestCount = 128;

uint8_t completionCode = 0;
uint32_t nextRecordHandle = 0;
uint32_t nextDataTransferHandle = 0;
uint8_t transferFlag = 0;
uint16_t respCount = 0;
uint8_t transferCRC = 0;

std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) + PLDM_GET_PDR_REQ_BYTES);
std::vector<uint8_t> responseMsg;
auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

void RemotePDRHandler::fetchPDR(std::vector<uint32_t> recordHandle)
{
    int fd = pldm_open();
    if (-1 == fd)
    {
        std::cerr << "failed to init mctp "
                  << "\n";
        return;
    }

    for (std::vector<uint32_t>::size_type i = 0; i < recordHandle.size(); i++)
    {
        uint8_t* responseMessage = nullptr;
        size_t responseMessageSize{};

        auto rc = encode_get_pdr_req(request->hdr.instance_id, recordHandle[i],
                                     0, PLDM_GET_FIRSTPART, requestCount, 0,
                                     request, PLDM_GET_PDR_REQ_BYTES);

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

        pdr.resize(respCount);
        pdrs.emplace_back(std::move(pdr));

        if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)completionCode
                      << std::endl;
            return;
        }
    }
}
} // namespace pldm
