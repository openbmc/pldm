#include "terminus_manager.hpp"

#include "manager.hpp"

namespace pldm
{
namespace platform_mc
{

std::optional<MctpInfo> TerminusManager::toMctpInfo(const tid_t& tid)
{
    if (transportLayerTable[tid] != SupportedTransportLayer::MCTP)
    {
        return std::nullopt;
    }

    auto it = mctpInfoTable.find(tid);
    if (it == mctpInfoTable.end())
    {
        return std::nullopt;
    }

    return it->second;
}

std::optional<tid_t> TerminusManager::toTid(const MctpInfo& mctpInfo)
{
    auto mctpInfoTableIterator = std::find_if(
        mctpInfoTable.begin(), mctpInfoTable.end(), [&mctpInfo](auto& v) {
            return (std::get<0>(v.second) == std::get<0>(mctpInfo)) &&
                   (std::get<1>(v.second) == std::get<1>(mctpInfo));
        });
    if (mctpInfoTableIterator == mctpInfoTable.end())
    {
        return std::nullopt;
    }
    return mctpInfoTableIterator->first;
}

std::optional<tid_t> TerminusManager::mapTid(const MctpInfo& mctpInfo,
                                             tid_t tid)
{
    if (tidPool[tid])
    {
        return std::nullopt;
    }

    tidPool[tid] = true;
    transportLayerTable[tid] = SupportedTransportLayer::MCTP;
    mctpInfoTable[tid] = mctpInfo;

    return tid;
}

std::optional<tid_t> TerminusManager::mapTid(const MctpInfo& mctpInfo)
{
    if (std::get<1>(mctpInfo) == 0 || std::get<1>(mctpInfo) == 0xff)
    {
        return std::nullopt;
    }

    auto mctpInfoTableIterator = std::find_if(
        mctpInfoTable.begin(), mctpInfoTable.end(), [&mctpInfo](auto& v) {
            return (std::get<0>(v.second) == std::get<0>(mctpInfo)) &&
                   (std::get<1>(v.second) == std::get<1>(mctpInfo));
        });
    if (mctpInfoTableIterator != mctpInfoTable.end())
    {
        return mctpInfoTableIterator->first;
    }

    auto tidPoolIterator = std::find(tidPool.begin(), tidPool.end(), false);
    if (tidPoolIterator == tidPool.end())
    {
        return std::nullopt;
    }

    tid_t tid = std::distance(tidPool.begin(), tidPoolIterator);
    return mapTid(mctpInfo, tid);
}

void TerminusManager::unmapTid(const tid_t& tid)
{
    if (tid == 0 || tid == PLDM_TID_RESERVED)
    {
        return;
    }
    tidPool[tid] = false;

    auto transportLayerTableIterator = transportLayerTable.find(tid);
    transportLayerTable.erase(transportLayerTableIterator);

    auto mctpInfoTableIterator = mctpInfoTable.find(tid);
    mctpInfoTable.erase(mctpInfoTableIterator);
}

requester::Coroutine TerminusManager::discoverMctpTerminusTask()
{
    while (!queuedMctpInfos.empty())
    {
        const MctpInfos& mctpInfos = queuedMctpInfos.front();
        // remove absent terminus
        for (auto it = termini.begin(); it != termini.end();)
        {
            bool found = false;
            for (auto& mctpInfo : mctpInfos)
            {
                if (mctpInfo == toMctpInfo(it->first))
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                unmapTid(it->first);
                termini.erase(it++);
            }
            else
            {
                it++;
            }
        }

        for (auto& mctpInfo : mctpInfos)
        {
            auto tid = toTid(mctpInfo);
            if (tid)
            {
                continue;
            }
            co_await initMctpTerminus(mctpInfo);
        }
        queuedMctpInfos.pop();
    }
}

requester::Coroutine TerminusManager::initMctpTerminus(const MctpInfo& mctpInfo)
{
    mctp_eid_t eid = std::get<1>(mctpInfo);
    tid_t tid = 0;
    auto rc = co_await getTidOverMctp(eid, tid);
    if (rc || tid == PLDM_TID_RESERVED)
    {
        co_return PLDM_ERROR;
    }

    if (tid == 0)
    {
        // unassigned tid
        auto mappedTid = mapTid(mctpInfo);
        if (!mappedTid)
        {
            co_return PLDM_ERROR;
        }
        tid = mappedTid.value();
        rc = co_await setTidOverMctp(eid, tid);
        if (rc)
        {
            unmapTid(tid);
            co_return rc;
        }
    }
    else
    {
        // check if terminus supports setTID
        rc = co_await setTidOverMctp(eid, tid);
        if (!rc)
        {
            // re-assign TID if it is not matched to the mapped value.
            auto mappedMctpInfo = toMctpInfo(tid);
            if (!mappedMctpInfo)
            {
                // check if mappedMctpInfo == mctpInfo, networkid?=  eid?=

                auto mappedTid = mapTid(mctpInfo);
                if (!mappedTid)
                {
                    co_return PLDM_ERROR;
                }

                tid = mappedTid.value();
                rc = co_await setTidOverMctp(eid, tid);
                if (rc)
                {
                    unmapTid(tid);
                    co_return rc;
                }
            }
        }
        else if (rc == PLDM_ERROR_UNSUPPORTED_PLDM_CMD)
        {
            auto mappedTid = mapTid(mctpInfo, tid);
            if (!mappedTid)
            {
                co_return PLDM_ERROR;
            }
        }
        else
        {
            co_return rc;
        }
    }

    uint64_t supportedTypes = 0;
    rc = co_await getPLDMTypes(tid, supportedTypes);
    if (rc)
    {
        co_return PLDM_ERROR;
    }

    auto terminus = std::make_shared<Terminus>(tid, supportedTypes);

    if (terminus->doesSupport(PLDM_PLATFORM))
    {
        rc = co_await getPDRs(terminus);
    }
    termini[tid] = terminus;

    co_return PLDM_SUCCESS;
}

requester::Coroutine TerminusManager::getTidOverMctp(mctp_eid_t eid, tid_t& tid)
{
    auto instanceId = requester.getInstanceId(eid);
    Request request(sizeof(pldm_msg_hdr));
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
    auto rc = encode_get_tid_req(instanceId, requestMsg);
    if (rc)
    {
        requester.markFree(eid, instanceId);
        std::cerr << "encode_get_tid_req failed. rc="
                  << static_cast<unsigned>(rc) << "\n";
        co_return rc;
    }

    const pldm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await requester::SendRecvPldmMsg<RequesterHandler>(
        handler, eid, request, &responseMsg, &responseLen);
    if (rc)
    {
        std::cerr << "sendRecvPldmMsg failed. rc=" << static_cast<unsigned>(rc)
                  << "\n";
        co_return rc;
    }

    uint8_t completionCode = 0;
    rc = decode_get_tid_resp(responseMsg, responseLen, &completionCode, &tid);
    if (rc)
    {
        std::cerr << "decode_get_tid_resp failed. rc="
                  << static_cast<unsigned>(rc) << "\n";
        co_return rc;
    }

    co_return completionCode;
}

requester::Coroutine TerminusManager::setTidOverMctp(mctp_eid_t eid, tid_t tid)
{
    auto instanceId = requester.getInstanceId(eid);
    Request request(sizeof(pldm_msg_hdr) + sizeof(pldm_set_tid_req));
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
    auto rc = encode_set_tid_req(instanceId, tid, requestMsg);
    if (rc)
    {
        requester.markFree(eid, instanceId);
        co_return rc;
    }

    const pldm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await requester::SendRecvPldmMsg<RequesterHandler>(
        handler, eid, request, &responseMsg, &responseLen);
    if (rc)
    {
        co_return rc;
    }

    co_return responseMsg->payload[0];
}

requester::Coroutine
    TerminusManager::getPDRs(std::shared_ptr<Terminus> terminus)
{
    tid_t tid = terminus->getTid();

    uint32_t recordHndl = 0;
    uint32_t nextRecordHndl = 0;
    uint32_t nextDataTransferHndl = 0;
    uint8_t transferFlag = 0;
    uint16_t responseCnt = 0;
    constexpr uint16_t recvBufSize = 1024;
    std::vector<uint8_t> recvBuf(recvBufSize);
    uint8_t transferCrc = 0;

    do
    {
        auto rc =
            co_await getPDR(tid, recordHndl, 0, PLDM_GET_FIRSTPART, recvBufSize,
                            0, nextRecordHndl, nextDataTransferHndl,
                            transferFlag, responseCnt, recvBuf, transferCrc);

        if (rc)
        {
            co_return rc;
        }

        if (transferFlag == PLDM_START || transferFlag == PLDM_START_AND_END)
        {
            // single-part transfer
            terminus->pdrs.emplace_back(std::vector<uint8_t>(
                recvBuf.begin(), recvBuf.begin() + responseCnt));
            recordHndl = nextRecordHndl;
        }
        else
        {
            // multipart transfer
            auto pdrHdr = reinterpret_cast<pldm_pdr_hdr*>(recvBuf.data());
            uint16_t recordChgNum = pdrHdr->record_change_num;
            std::vector<uint8_t> receivedPdr(recvBuf.begin(),
                                             recvBuf.begin() + responseCnt);
            do
            {
                rc = co_await getPDR(tid, recordHndl, nextDataTransferHndl,
                                     PLDM_GET_NEXTPART, recvBufSize,
                                     recordChgNum, nextRecordHndl,
                                     nextDataTransferHndl, transferFlag,
                                     responseCnt, recvBuf, transferCrc);
                if (rc)
                {
                    co_return rc;
                }

                receivedPdr.insert(receivedPdr.end(), recvBuf.begin(),
                                   recvBuf.begin() + responseCnt);

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

requester::Coroutine TerminusManager::getPLDMTypes(tid_t tid,
                                                   uint64_t& supportedTypes)
{
    Request request(sizeof(pldm_msg_hdr));
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
    auto rc = encode_get_types_req(0, requestMsg);
    if (rc)
    {
        std::cerr << "encode_get_types_req failed. rc="
                  << static_cast<unsigned>(rc) << "\n";
        co_return rc;
    }

    const pldm_msg* responseMsg = NULL;
    size_t responseLen = 0;

    rc = co_await SendRecvPldmMsg(tid, request, &responseMsg, &responseLen);
    if (rc)
    {
        co_return rc;
    }

    uint8_t completionCode = 0;
    bitfield8_t* types = reinterpret_cast<bitfield8_t*>(&supportedTypes);
    rc =
        decode_get_types_resp(responseMsg, responseLen, &completionCode, types);
    if (rc)
    {
        std::cerr << "decode_get_types_resp failed. rc="
                  << static_cast<unsigned>(rc) << "\n";
        co_return rc;
    }
    co_return completionCode;
}

requester::Coroutine TerminusManager::getPDR(
    tid_t tid, uint32_t recordHndl, uint32_t dataTransferHndl,
    uint8_t transferOpFlag, uint16_t requestCnt, uint16_t recordChgNum,
    uint32_t& nextRecordHndl, uint32_t& nextDataTransferHndl,
    uint8_t& transferFlag, uint16_t& responseCnt,
    std::vector<uint8_t>& recordData, uint8_t& transferCrc)
{
    Request request(sizeof(pldm_msg_hdr) + PLDM_GET_PDR_REQ_BYTES);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
    auto rc = encode_get_pdr_req(0, recordHndl, dataTransferHndl,
                                 transferOpFlag, requestCnt, recordChgNum,
                                 requestMsg, PLDM_GET_PDR_REQ_BYTES);
    if (rc)
    {
        co_return rc;
    }

    const pldm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await SendRecvPldmMsg(tid, request, &responseMsg, &responseLen);
    if (rc)
    {
        co_return rc;
    }

    uint8_t completionCode;
    rc = decode_get_pdr_resp(responseMsg, responseLen, &completionCode,
                             &nextRecordHndl, &nextDataTransferHndl,
                             &transferFlag, &responseCnt, recordData.data(),
                             recordData.size(), &transferCrc);
    if (rc)
    {
        co_return rc;
    }
    co_return completionCode;
}

requester::Coroutine
    TerminusManager::SendRecvPldmMsg(tid_t tid, Request& request,
                                     const pldm_msg** responseMsg,
                                     size_t* responseLen)
{
    if (tidPool[tid] == SupportedTransportLayer::MCTP)
    {
        auto mctpInfo = toMctpInfo(tid);
        if (!mctpInfo)
        {
            co_return PLDM_ERROR;
        }

        auto eid = std::get<1>(mctpInfo.value());
        auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
        requestMsg->hdr.instance_id = requester.getInstanceId(eid);
        auto rc = co_await requester::SendRecvPldmMsg<RequesterHandler>(
            handler, eid, request, responseMsg, responseLen);
        if (rc)
        {
            std::cerr << "SendRecvPldmMsg failed. rc="
                      << static_cast<unsigned>(rc) << "\n";
        }
        co_return rc;
    }
    else
    {
        co_return PLDM_ERROR;
    }
}

} // namespace platform_mc
} // namespace pldm
