#include "terminus.hpp"

#include "platform.h"

#include "terminus_manager.hpp"

namespace pldm
{
namespace platform_mc
{

void Terminus::fetchPDRs()
{
    deferredFetchPDREvent = std::make_unique<sdeventplus::source::Defer>(
        event, std::bind(std::mem_fn((&Terminus::processFetchPDREvent)), this,
                         0, std::placeholders::_1));
}

void Terminus::processFetchPDREvent(
    uint32_t nextRecordHandle, sdeventplus::source::EventBase& /* source */)
{
    deferredFetchPDREvent.reset();
    this->sendGetPDR(nextRecordHandle, 0, PLDM_GET_FIRSTPART, UINT16_MAX, 0);
}

void Terminus::sendGetPDR(uint32_t record_hndl, uint32_t data_transfer_hndl,
                          uint8_t op_flag, uint16_t request_cnt,
                          uint16_t record_chg_num)
{
    auto instanceId = requester.getInstanceId(eid);
    Request requestMsg(sizeof(pldm_msg_hdr) + PLDM_GET_PDR_REQ_BYTES);

    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_get_pdr_req(instanceId, record_hndl, data_transfer_hndl,
                                 op_flag, request_cnt, record_chg_num, request,
                                 PLDM_GET_PDR_REQ_BYTES);

    if (rc != PLDM_SUCCESS)
    {
        requester.markFree(eid, instanceId);
        std::cerr << "Failed to encode_get_pdr_req, EID=" << unsigned(eid)
                  << ", RC=" << rc << std::endl;
        return manager->initTerminus();
    }

    rc = handler.registerRequest(
        eid, instanceId, PLDM_PLATFORM, PLDM_GET_PDR, std::move(requestMsg),
        std::move(std::bind_front(&Terminus::handleRespGetPDR, this)));
    if (rc)
    {
        std::cerr << "Failed to send GetPDR request, EID=" << unsigned(eid)
                  << ", RC=" << rc << std::endl;
        return manager->initTerminus();
    }
}

void Terminus::handleRespGetPDR(mctp_eid_t _eid, const pldm_msg* response,
                                size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        std::cerr << "No response received for getPDR, EID=" << unsigned(_eid)
                  << std::endl;
        return manager->initTerminus();
    }

    uint8_t completionCode = PLDM_SUCCESS;
    uint32_t next_record_hndl;
    uint32_t next_data_transfer_hndl;
    uint8_t transfer_flag;
    uint16_t resp_cnt;
    uint8_t transfer_crc;

    auto rc = decode_get_pdr_resp(response, respMsgLen, &completionCode,
                                  &next_record_hndl, &next_data_transfer_hndl,
                                  &transfer_flag, &resp_cnt, nullptr, 0,
                                  &transfer_crc);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Failed to decode_get_pdr_resp, EID=" << unsigned(_eid)
                  << ", RC=" << rc << std::endl;
        return manager->initTerminus();
    }

    std::vector<uint8_t> pdr(resp_cnt, 0);
    rc = decode_get_pdr_resp(response, respMsgLen, &completionCode,
                             &next_record_hndl, &next_data_transfer_hndl,
                             &transfer_flag, &resp_cnt, pdr.data(), resp_cnt,
                             &transfer_crc);
    if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
    {
        std::cerr << "Failed to decode_get_pdr_resp, EID=" << unsigned(_eid)
                  << ", RC =" << rc << ", CC=" << unsigned(completionCode)
                  << std::endl;
        return manager->initTerminus();
    }

    auto pdrHdr = reinterpret_cast<pldm_pdr_hdr*>(pdr.data());
    if (pdrHdr->type == PLDM_NUMERIC_SENSOR_PDR)
    {
        numericSensorPDRs.emplace_back(pdr);
    }
    else if (pdrHdr->type == PLDM_ENTITY_AUXILIARY_NAMES_PDR)
    {
        entityAuxiliaryNamePDRs.emplace_back(pdr);
    }
    else
    {
        std::cerr << "found a pdr which is not support yet" << std::endl;
    }

    if (next_record_hndl)
    {
        deferredFetchPDREvent = std::make_unique<sdeventplus::source::Defer>(
            event, std::bind(std::mem_fn((&Terminus::processFetchPDREvent)),
                             this, next_record_hndl, std::placeholders::_1));
    }
    else
    {
        inventoryPath =
            "/xyz/openbmc_project/inventory/Item/Board/PLDM_Device_" +
            std::to_string(tid);
        inventoryItemBoardInft = std::make_shared<InventoryItemBoardIntf>(
            utils::DBusHandler::getBus(), inventoryPath.c_str());
        parsePDRs();
        return manager->initTerminus();
    }
}

void Terminus::parsePDRs()
{}

} // namespace platform_mc
} // namespace pldm
