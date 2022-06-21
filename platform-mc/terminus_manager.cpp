#include "terminus_manager.hpp"

#include "manager.hpp"

namespace pldm
{
namespace platform_mc
{

std::optional<uint8_t> TerminusManager::toEID(uint8_t tid)
{
    if (tid != 0 && tid != tidPoolSize && tidPool.at(tid))
    {
        return tidPool.at(tid);
    }
    return std::nullopt;
}

std::optional<uint8_t> TerminusManager::toTID(uint8_t eid)
{
    for (size_t i = 1; i < tidPool.size(); i++)
    {
        if (tidPool.at(i) == eid)
        {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<uint8_t> TerminusManager::mapToTID(uint8_t eid)
{
    if (eid == 0 || eid == 0xff)
    {
        return std::nullopt;
    }

    auto mappedTid = toTID(eid);
    if (mappedTid)
    {
        return mappedTid.value();
    }

    for (size_t i = 1; i < tidPool.size(); i++)
    {
        // find a free slot
        if (tidPool.at(i) == PLDM_EID_NULL)
        {
            tidPool.at(i) = eid;
            return i;
        }
    }
    return std::nullopt;
}

void TerminusManager::unmapTID(uint8_t tid)
{
    tidPool.at(tid) = PLDM_EID_NULL;
}

requester::Coroutine
    TerminusManager::discoverTerminusTask(const MctpInfos& mctpInfos)
{
    // remove absent terminus
    for (auto it = termini.begin(); it != termini.end();)
    {
        bool found = false;
        for (auto& mctpInfo : mctpInfos)
        {
            if (mctpInfo.first == it->first)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            termini.erase(it++);
        }
        else
        {
            it++;
        }
    }

    for (auto& mctpInfo : mctpInfos)
    {
        auto it = termini.find(mctpInfo.first);
        if (it != termini.end())
        {
            continue;
        }
        co_await initTerminus(mctpInfo);
    }
}

requester::Coroutine TerminusManager::initTerminus(const MctpInfo& mctpInfo)
{
    uint8_t eid = mctpInfo.first;
    uint64_t supportedTypes = 0;
    auto rc = co_await getPLDMTypes(eid, supportedTypes);
    if (rc)
    {
        co_return PLDM_ERROR;
    }

    uint8_t tid = 0;
    rc = co_await getTID(eid, tid);
    if (rc || tid == PLDM_TID_RESERVED)
    {
        co_return PLDM_ERROR;
    }

    if (tid == 0)
    {
        // unassigned TID
        auto mappedTID = mapToTID(eid);
        if (!mappedTID)
        {
            co_return PLDM_ERROR;
        }

        rc = co_await setTID(eid, mappedTID.value());
        if (rc)
        {
            unmapTID(tid);
            co_return rc;
        }
    }
    else
    {
        // check if terminus supports setTID
        rc = co_await setTID(eid, tid);
        if (!rc)
        {
            // re-assign TID if it is not matched to the mapped value.
            auto mappedEID = toEID(tid);
            if (!mappedEID || eid != mappedEID.value())
            {
                auto mappedTID = mapToTID(eid);
                if (!mappedTID)
                {
                    co_return PLDM_ERROR;
                }

                rc = co_await setTID(eid, mappedTID.value());
                if (rc)
                {
                    unmapTID(tid);
                    co_return rc;
                }
                tid = mappedTID.value();
            }
        }
        else if (rc != PLDM_ERROR_UNSUPPORTED_PLDM_CMD)
        {
            co_return rc;
        }
    }

    auto terminus = std::make_shared<Terminus>(eid, tid, supportedTypes);
    if (terminus->doesSupport(PLDM_PLATFORM))
    {
        rc = co_await getPDRs(terminus);
        if (!rc)
        {
            terminus->parsePDRs();
        }
    }
    termini[eid] = terminus;

    co_return PLDM_SUCCESS;
}

requester::Coroutine TerminusManager::getTID(mctp_eid_t eid, uint8_t& tid)
{
    auto instanceId = requester.getInstanceId(eid);
    Request requestMsg(sizeof(pldm_msg_hdr));
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_get_tid_req(instanceId, request);
    if (rc)
    {
        requester.markFree(eid, instanceId);
        std::cerr << "encode_get_tid_req failed. rc="
                  << static_cast<unsigned>(rc) << "\n";
        co_return rc;
    }

    Response responseMsg{};
    rc = co_await requester::sendRecvPldmMsg(handler, eid, requestMsg,
                                             responseMsg);
    if (rc)
    {
        std::cerr << "sendRecvPldmMsg failed. rc=" << static_cast<unsigned>(rc)
                  << "\n";
        co_return rc;
    }

    uint8_t completionCode = 0;
    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());
    auto length = responseMsg.size() - sizeof(struct pldm_msg_hdr);
    rc = decode_get_tid_resp(response, length, &completionCode, &tid);
    if (rc)
    {
        std::cerr << "decode_get_tid_resp failed. rc="
                  << static_cast<unsigned>(rc) << "\n";
        co_return rc;
    }

    co_return completionCode;
}

requester::Coroutine TerminusManager::getPLDMTypes(mctp_eid_t eid,
                                                   uint64_t& supportedTypes)
{
    auto instanceId = requester.getInstanceId(eid);
    Request requestMsg(sizeof(pldm_msg_hdr));
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_get_types_req(instanceId, request);
    if (rc)
    {
        requester.markFree(eid, instanceId);
        std::cerr << "encode_get_types_req failed. rc="
                  << static_cast<unsigned>(rc) << "\n";
        co_return rc;
    }

    Response responseMsg{};
    rc = co_await requester::sendRecvPldmMsg(handler, eid, requestMsg,
                                             responseMsg);
    if (rc)
    {
        std::cerr << "sendRecvPldmMsg failed. rc=" << static_cast<unsigned>(rc)
                  << "\n";
        co_return rc;
    }

    uint8_t completionCode = 0;
    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());
    auto length = responseMsg.size() - sizeof(struct pldm_msg_hdr);
    bitfield8_t* types = reinterpret_cast<bitfield8_t*>(&supportedTypes);
    rc = decode_get_types_resp(response, length, &completionCode, types);
    if (rc)
    {
        std::cerr << "decode_get_types_resp failed. rc="
                  << static_cast<unsigned>(rc) << "\n";
        co_return rc;
    }
    co_return completionCode;
}

requester::Coroutine TerminusManager::setTID(mctp_eid_t eid, uint8_t tid)
{
    auto instanceId = requester.getInstanceId(eid);
    Request requestMsg(sizeof(pldm_msg_hdr) + sizeof(pldm_set_tid_req));
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_set_tid_req(instanceId, tid, request);
    if (rc)
    {
        requester.markFree(eid, instanceId);
        co_return rc;
    }

    Response responseMsg{};
    rc = co_await requester::sendRecvPldmMsg(handler, eid, requestMsg,
                                             responseMsg);
    if (rc)
    {
        co_return rc;
    }

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());
    co_return response->payload[0];
}

requester::Coroutine
    TerminusManager::getPDRs(std::shared_ptr<Terminus> terminus)
{
    mctp_eid_t eid = terminus->eid();
    constexpr uint16_t requestCnt = 1024;
    uint32_t recordHndl = 0;

    uint32_t nextRecordHndl = 0;
    uint32_t nextDataTransferHndl = 0;
    uint8_t transferFlag = 0;
    uint16_t responseCnt = 0;
    std::vector<uint8_t> recordData(requestCnt);
    uint8_t transferCrc = 0;

    do
    {
        auto rc =
            co_await getPDR(eid, recordHndl, 0, PLDM_GET_FIRSTPART, requestCnt,
                            0, nextRecordHndl, nextDataTransferHndl,
                            transferFlag, responseCnt, recordData, transferCrc);

        if (rc)
        {
            co_return rc;
        }

        if (transferFlag == PLDM_START || transferFlag == PLDM_START_AND_END)
        {
            // single-part transfer
            terminus->pdrs.emplace_back(std::vector<uint8_t>(
                recordData.begin(), recordData.begin() + responseCnt));
            recordHndl = nextRecordHndl;
        }
        else
        {
            // multipart transfer
            auto pdrHdr = reinterpret_cast<pldm_pdr_hdr*>(recordData.data());
            uint16_t recordChgNum = pdrHdr->record_change_num;
            std::vector<uint8_t> receivedPdr(recordData.begin(),
                                             recordData.begin() + responseCnt);
            do
            {
                rc = co_await getPDR(eid, recordHndl, nextDataTransferHndl,
                                     PLDM_GET_NEXTPART, requestCnt,
                                     recordChgNum, nextRecordHndl,
                                     nextDataTransferHndl, transferFlag,
                                     responseCnt, recordData, transferCrc);
                if (rc)
                {
                    co_return rc;
                }

                receivedPdr.insert(receivedPdr.end(), recordData.begin(),
                                   recordData.begin() + responseCnt);

                if (transferFlag == PLDM_END)
                {
                    terminus->pdrs.emplace_back(receivedPdr);
                    recordHndl = nextRecordHndl;
                }
            } while (nextDataTransferHndl != 0);
        }
    } while (nextRecordHndl != 0);

    co_return PLDM_SUCCESS;
}

requester::Coroutine TerminusManager::getPDR(
    mctp_eid_t eid, uint32_t recordHndl, uint32_t dataTransferHndl,
    uint8_t transferOpFlag, uint16_t requestCnt, uint16_t recordChgNum,
    uint32_t& nextRecordHndl, uint32_t& nextDataTransferHndl,
    uint8_t& transferFlag, uint16_t& responseCnt,
    std::vector<uint8_t>& recordData, uint8_t& transferCrc)
{
    auto instanceId = requester.getInstanceId(eid);
    Request requestMsg(sizeof(pldm_msg_hdr) + PLDM_GET_PDR_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_get_pdr_req(instanceId, recordHndl, dataTransferHndl,
                                 transferOpFlag, requestCnt, recordChgNum,
                                 request, PLDM_GET_PDR_REQ_BYTES);
    if (rc)
    {
        requester.markFree(eid, instanceId);
        co_return rc;
    }

    Response responseMsg{};
    rc = co_await requester::sendRecvPldmMsg(handler, eid, requestMsg,
                                             responseMsg);
    if (rc)
    {
        co_return rc;
    }

    auto msg = reinterpret_cast<pldm_msg*>(responseMsg.data());
    size_t payloadLength = responseMsg.size() - sizeof(struct pldm_msg_hdr);
    uint8_t completionCode;
    rc = decode_get_pdr_resp(msg, payloadLength, &completionCode,
                             &nextRecordHndl, &nextDataTransferHndl,
                             &transferFlag, &responseCnt, recordData.data(),
                             recordData.size(), &transferCrc);
    if (rc)
    {
        co_return rc;
    }
    co_return completionCode;
}

} // namespace platform_mc
} // namespace pldm
