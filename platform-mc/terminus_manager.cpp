#include "terminus_manager.hpp"

#include "manager.hpp"

namespace pldm
{
namespace platform_mc
{

std::optional<MctpInfos> TerminusManager::toMctpInfos(const pldm_tid_t& tid)
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

std::optional<pldm_tid_t> TerminusManager::toTid(const MctpInfo& mctpInfo)
{
    for (auto its : mctpInfoTable)
    {
        auto& mctpInfos = its.second;
        auto iter = std::find(mctpInfos.begin(), mctpInfos.end(), mctpInfo);
        if (iter != mctpInfos.end())
        {
            return its.first;
        }
    }

    return std::nullopt;
}

std::optional<pldm_tid_t> TerminusManager::mapTid(const MctpInfo& mctpInfo,
                                                  pldm_tid_t tid)
{
    if (std::get<0>(mctpInfo) == 0 || std::get<0>(mctpInfo) == 0xff)
    {
        return std::nullopt;
    }

    if (tidPool[tid])
    {
        /* Add new medium interface to terminus` interfaces list*/
        if (manager)
        {
            manager->stopSensorPolling(tid);
        }
        auto mctpInfoTableIterator = mctpInfoTable.find(tid);
        mctpInfoTableIterator->second.push_back(mctpInfo);
        mctpInfoTable[tid] = mctpInfoTableIterator->second;
        /* Recover the sensor polling on latest added medium interface*/
        if (manager)
        {
            manager->startSensorPolling(tid);
        }
        return tid;
    }

    tidPool[tid] = true;
    transportLayerTable[tid] = SupportedTransportLayer::MCTP;
    mctpInfoTable[tid] = {mctpInfo};

    return tid;
}

std::optional<pldm_tid_t> TerminusManager::mapTid(const MctpInfo& mctpInfo)
{
    if (std::get<0>(mctpInfo) == 0 || std::get<0>(mctpInfo) == 0xff)
    {
        return std::nullopt;
    }

    for (auto its : mctpInfoTable)
    {
        auto& mctpInfos = its.second;
        auto iter = std::find(mctpInfos.begin(), mctpInfos.end(), mctpInfo);
        if (iter != mctpInfos.end())
        {
            return its.first;
        }
    }

    auto tidPoolIterator = std::find(tidPool.begin(), tidPool.end(), false);
    if (tidPoolIterator == tidPool.end())
    {
        return std::nullopt;
    }

    pldm_tid_t tid = std::distance(tidPool.begin(), tidPoolIterator);
    return mapTid(mctpInfo, tid);
}

bool TerminusManager::unmapTid(const pldm_tid_t& tid)
{
    if (tid == 0 || tid == PLDM_TID_RESERVED)
    {
        return false;
    }
    tidPool[tid] = false;
    auto transportLayerTableIterator = transportLayerTable.find(tid);
    transportLayerTable.erase(transportLayerTableIterator);
    auto mctpInfoTableIterator = mctpInfoTable.find(tid);
    mctpInfoTable.erase(mctpInfoTableIterator);

    return true;
}

bool TerminusManager::unmapMctpInfo(const pldm_tid_t& tid,
                                    const MctpInfo& mctpInfo)
{
    if (tid == 0 || tid == PLDM_TID_RESERVED)
    {
        return false;
    }
    auto mctpInfoTableIterator = mctpInfoTable.find(tid);

    if (mctpInfoTableIterator == mctpInfoTable.end())
    {
        return false;
    }

    auto mctpInfos = mctpInfoTableIterator->second;
    if (mctpInfos.size() > 1)
    {
        auto iter = std::find(mctpInfos.begin(), mctpInfos.end(), mctpInfo);
        if (iter != mctpInfos.end())
        {
            mctpInfos.erase(iter);
            mctpInfoTable[tid] = mctpInfos;
            return true;
        }
        return false;
    }
    else
    {
        auto iter = std::find(mctpInfos.begin(), mctpInfos.end(), mctpInfo);
        if (iter == mctpInfos.end())
        {
            return false;
        }
        return unmapTid(tid);
    }

    return true;
}

void TerminusManager::discoverMctpTerminus(const MctpInfos& mctpInfos)
{
    queuedMctpInfos.emplace(mctpInfos);
    if (discoverMctpTerminusTaskHandle.has_value())
    {
        auto& [scope, rcOpt] = *discoverMctpTerminusTaskHandle;
        if (!rcOpt.has_value())
        {
            return;
        }
        stdexec::sync_wait(scope.on_empty());
        discoverMctpTerminusTaskHandle.reset();
    }
    auto& [scope, rcOpt] = discoverMctpTerminusTaskHandle.emplace();
    scope.spawn(discoverMctpTerminusTask() |
                    stdexec::then([&](int rc) { rcOpt.emplace(rc); }),
                exec::default_task_context<void>());
}

std::map<pldm_tid_t, std::shared_ptr<Terminus>>::iterator
    TerminusManager::findTeminusPtr(const MctpInfo& mctpInfo)
{
    bool found = false;
    auto it = termini.begin();
    for (; it != termini.end();)
    {
        auto terminusMctpInfos = toMctpInfos(it->first);
        if (terminusMctpInfos)
        {
            auto& mctpInfos = *terminusMctpInfos;
            auto iter = std::find(mctpInfos.begin(), mctpInfos.end(), mctpInfo);
            if (iter != mctpInfos.end())
            {
                found = true;
                break;
            }
        }
        it++;
    }
    if (found)
    {
        return it;
    }

    return termini.end();
}

exec::task<int> TerminusManager::discoverMctpTerminusTask()
{
    std::vector<pldm_tid_t> addedTids;
    while (!queuedMctpInfos.empty())
    {
        if (manager)
        {
            co_await manager->beforeDiscoverTerminus();
        }

        const MctpInfos& mctpInfos = queuedMctpInfos.front();
        for (auto& mctpInfo : mctpInfos)
        {
            auto it = findTeminusPtr(mctpInfo);
            if (it == termini.end())
            {
                co_await initMctpTerminus(mctpInfo);
            }

            /* Get TID of initialized terminus */
            auto tid = toTid(mctpInfo);
            if (!tid)
            {
                co_return PLDM_ERROR;
            }
            addedTids.push_back(tid.value());
        }

        if (manager)
        {
            co_await manager->afterDiscoverTerminus();
            for (auto& tid : addedTids)
            {
                manager->startSensorPolling(tid);
            }
        }

        queuedMctpInfos.pop();
    }

    co_return PLDM_SUCCESS;
}

void TerminusManager::removeMctpTerminus(const MctpInfos& mctpInfos)
{
    // remove terminus
    for (auto& mctpInfo : mctpInfos)
    {
        auto it = findTeminusPtr(mctpInfo);
        if (it == termini.end())
        {
            continue;
        }

        if (manager)
        {
            manager->stopSensorPolling(it->first);
        }

        auto terminusMctpInfos = toMctpInfos(it->first);
        /* Terminus only has one medium interface */
        if (terminusMctpInfos->size() == 1)
        {
            unmapTid(it->first);
            termini.erase(it);
        }
        else if (terminusMctpInfos->size() == 0)
        {
            continue;
        }
        /* Terminus has multiple medium interfaces */
        else
        {
            unmapMctpInfo(it->second->getTid(), mctpInfo);
            /* Recover the sensor polling on latest added medium interface*/
            if (manager)
            {
                manager->startSensorPolling(it->second->getTid());
            }
        }
    }
}

exec::task<int> TerminusManager::initMctpTerminus(const MctpInfo& mctpInfo)
{
    mctp_eid_t eid = std::get<0>(mctpInfo);
    pldm_tid_t tid = 0;
    bool isMapped = false;
    bool isDiscovery = false;
    auto rc = co_await getTidOverMctp(eid, &tid);
    if (rc || tid == PLDM_TID_RESERVED)
    {
        co_return PLDM_ERROR;
    }

    /* Terminus already has TID */
    if (tid != 0x00)
    {
        auto it = termini.find(tid);
        /* TID is used by one discovered terminus */
        if (it != termini.end())
        {
            auto terminusMctpInfo = toMctpInfos(it->first);
            /* The discovered terminus has the same MCTP Info */
            if (terminusMctpInfo)
            {
                auto& mctpInfos = *terminusMctpInfo;
                auto iter = std::find(mctpInfos.begin(), mctpInfos.end(),
                                      mctpInfo);
                if (iter != mctpInfos.end())
                {
                    co_return PLDM_SUCCESS;
                }
            }

            /* Terminus supports multiple medium interfaces */
            auto mappedTid = mapTid(mctpInfo, tid);
            if (!mappedTid)
            {
                co_return PLDM_ERROR;
            }
            isMapped = true;
            isDiscovery = true;
        }
        /* Use the terminus TID for mapping */
        else
        {
            auto mappedTid = mapTid(mctpInfo, tid);
            if (!mappedTid)
            {
                co_return PLDM_ERROR;
            }
            isMapped = true;
        }
    }

    if (!isMapped)
    {
        // Assigning a tid. If it has been mapped, mapTid()
        // returns the tid assigned before.
        auto mappedTid = mapTid(mctpInfo);
        if (!mappedTid)
        {
            co_return PLDM_ERROR;
        }

        tid = mappedTid.value();
        rc = co_await setTidOverMctp(eid, tid);
        if (rc != PLDM_SUCCESS && rc != PLDM_ERROR_UNSUPPORTED_PLDM_CMD)
        {
            unmapTid(tid);
            co_return rc;
        }

        auto it = termini.find(tid);
        if (it != termini.end())
        {
            // the terminus has been discovered before
            co_return PLDM_SUCCESS;
        }
    }

    /* Discovery the mapped terminus */
    if (!isDiscovery)
    {
        uint64_t supportedTypes = 0;
        rc = co_await getPLDMTypes(tid, supportedTypes);
        if (rc)
        {
            std::cerr << "failed to get PLDM Types\n";
            co_return PLDM_ERROR;
        }

        termini[tid] = std::make_shared<Terminus>(tid, supportedTypes);
    }

    co_return PLDM_SUCCESS;
}

exec::task<int>
    TerminusManager::SendRecvPldmMsgOverMctp(mctp_eid_t eid, Request& request,
                                             const pldm_msg** responseMsg,
                                             size_t* responseLen)
{
    try
    {
        std::tie(*responseMsg, *responseLen) =
            co_await handler.sendRecvMsg(eid, std::move(request));
        co_return PLDM_SUCCESS;
    }
    catch (int rc)
    {
        std::cerr << "sendRecvPldmMsgOverMctp failed. rc="
                  << static_cast<unsigned>(rc) << "\n";
        co_return rc;
    }
}

exec::task<int> TerminusManager::getTidOverMctp(mctp_eid_t eid, pldm_tid_t* tid)
{
    auto instanceId = instanceIdDb.next(eid);
    Request request(sizeof(pldm_msg_hdr));
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
    auto rc = encode_get_tid_req(instanceId, requestMsg);
    if (rc)
    {
        instanceIdDb.free(eid, instanceId);
        std::cerr << "encode_get_tid_req failed. rc="
                  << static_cast<unsigned>(rc) << "\n";
        co_return rc;
    }

    const pldm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await SendRecvPldmMsgOverMctp(eid, request, &responseMsg,
                                          &responseLen);
    if (rc)
    {
        co_return rc;
    }

    uint8_t completionCode = 0;
    rc = decode_get_tid_resp(responseMsg, responseLen, &completionCode, tid);
    if (rc)
    {
        std::cerr << "decode_get_tid_resp failed. rc="
                  << static_cast<unsigned>(rc) << "\n";
        co_return rc;
    }

    co_return completionCode;
}

exec::task<int> TerminusManager::setTidOverMctp(mctp_eid_t eid, pldm_tid_t tid)
{
    auto instanceId = instanceIdDb.next(eid);
    Request request(sizeof(pldm_msg_hdr) + sizeof(pldm_set_tid_req));
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
    auto rc = encode_set_tid_req(instanceId, tid, requestMsg);
    if (rc)
    {
        instanceIdDb.free(eid, instanceId);
        co_return rc;
    }

    const pldm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await SendRecvPldmMsgOverMctp(eid, request, &responseMsg,
                                          &responseLen);
    if (rc)
    {
        co_return rc;
    }

    if (responseMsg == NULL || responseLen != PLDM_SET_TID_RESP_BYTES)
    {
        co_return PLDM_ERROR_INVALID_LENGTH;
    }

    co_return responseMsg->payload[0];
}

exec::task<int> TerminusManager::getPLDMTypes(pldm_tid_t tid,
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
    rc = decode_get_types_resp(responseMsg, responseLen, &completionCode,
                               types);
    if (rc)
    {
        std::cerr << "decode_get_types_resp failed. rc="
                  << static_cast<unsigned>(rc) << "\n";
        co_return rc;
    }
    co_return completionCode;
}

exec::task<int> TerminusManager::SendRecvPldmMsg(pldm_tid_t tid,
                                                 Request& request,
                                                 const pldm_msg** responseMsg,
                                                 size_t* responseLen)
{
    if (tidPool[tid] &&
        transportLayerTable[tid] == SupportedTransportLayer::MCTP)
    {
        auto mctpInfos = toMctpInfos(tid);
        if (!mctpInfos)
        {
            co_return PLDM_ERROR;
        }
        /* Use latest added medium interface to send the pldm messages */
        auto eid = std::get<0>(mctpInfos->back());

        auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
        requestMsg->hdr.instance_id = instanceIdDb.next(eid);
        auto rc = co_await SendRecvPldmMsgOverMctp(eid, request, responseMsg,
                                                   responseLen);
        co_return rc;
    }
    else
    {
        co_return PLDM_ERROR;
    }
}

} // namespace platform_mc
} // namespace pldm
