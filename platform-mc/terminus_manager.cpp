#include "terminus_manager.hpp"

#include "manager.hpp"

namespace pldm
{
namespace platform_mc
{

using EpochTimeUS = uint64_t;

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
                   (std::get<3>(v.second) == std::get<3>(mctpInfo));
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
    if (std::get<0>(mctpInfo) == 0 || std::get<0>(mctpInfo) == 0xff)
    {
        return std::nullopt;
    }

    auto mctpInfoTableIterator = std::find_if(
        mctpInfoTable.begin(), mctpInfoTable.end(), [&mctpInfo](auto& v) {
            return (std::get<0>(v.second) == std::get<0>(mctpInfo)) &&
                   (std::get<3>(v.second) == std::get<3>(mctpInfo));
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

bool TerminusManager::unmapTid(const tid_t& tid)
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

void TerminusManager::discoverMctpTerminus(const MctpInfos& mctpInfos)
{
    queuedMctpInfos.emplace(mctpInfos);
    if (discoverMctpTerminusTaskHandle)
    {
        if (!discoverMctpTerminusTaskHandle.done())
        {
            return;
        }
        discoverMctpTerminusTaskHandle.destroy();
    }

    auto co = discoverMctpTerminusTask();
    discoverMctpTerminusTaskHandle = co.handle;
    if (discoverMctpTerminusTaskHandle.done())
    {
        discoverMctpTerminusTaskHandle = nullptr;
    }
}

requester::Coroutine TerminusManager::discoverMctpTerminusTask()
{
    if (manager)
    {
        manager->stopSensorPolling();
    }
    while (!queuedMctpInfos.empty())
    {
        if (manager)
        {
            co_await manager->beforeDiscoverTerminus();
        }

        const MctpInfos& mctpInfos = queuedMctpInfos.front();
        // remove absent terminus
        for (auto it = termini.begin(); it != termini.end();)
        {
            bool found = false;
            for (auto& mctpInfo : mctpInfos)
            {
                auto terminusMctpInfo = toMctpInfo(it->first);
                if (terminusMctpInfo &&
                    (std::get<0>(terminusMctpInfo.value()) ==
                     std::get<0>(mctpInfo)) &&
                    (std::get<1>(terminusMctpInfo.value()) ==
                     std::get<1>(mctpInfo)))
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
            co_await initMctpTerminus(mctpInfo);
        }

        if (manager)
        {
            co_await manager->afterDiscoverTerminus();
        }

        queuedMctpInfos.pop();
    }

    if (manager)
    {
        manager->startSensorPolling();
    }
    co_return PLDM_SUCCESS;
}

requester::Coroutine TerminusManager::setEventReceiver(uint8_t eid)
{
    std::cerr << "Discovery Terminus: " << unsigned(eid)
              << " get set Event Receiver." << std::endl;
    uint8_t eventMessageGlobalEnable =
        PLDM_EVENT_MESSAGE_GLOBAL_ENABLE_ASYNC_KEEP_ALIVE;
    uint8_t transportProtocolType = PLDM_TRANSPORT_PROTOCOL_TYPE_MCTP;
    uint8_t eventReceiverAddressInfo = BMC_EVENT_RECEIVER_ID;
    uint16_t heartbeatTimer = 0x78;

    auto instanceId = requester.getInstanceId(eid);
    Request request(sizeof(pldm_msg_hdr) + PLDM_SET_EVENT_RECEIVER_REQ_BYTES);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());

    auto rc = encode_set_event_receiver_req(
        instanceId, eventMessageGlobalEnable, transportProtocolType,
        eventReceiverAddressInfo, heartbeatTimer, requestMsg);
    if (rc != PLDM_SUCCESS)
    {
        requester.markFree(eid, instanceId);
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

    rc = decode_set_event_receiver_resp(responseMsg, responseLen,
                                        &completionCode);

    if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
    {
        co_return rc;
    }

    co_return completionCode;
}

void epochToBCDTime(const uint64_t& timeSec, uint8_t* seconds, uint8_t* minutes,
                    uint8_t* hours, uint8_t* day, uint8_t* month,
                    uint16_t* year)
{
    auto t = time_t(timeSec);
    auto time = localtime(&t);

    *seconds = (uint8_t)pldm::utils::decimalToBcd(time->tm_sec);
    *minutes = (uint8_t)pldm::utils::decimalToBcd(time->tm_min);
    *hours = (uint8_t)pldm::utils::decimalToBcd(time->tm_hour);
    *day = (uint8_t)pldm::utils::decimalToBcd(time->tm_mday);
    *month = (uint8_t)pldm::utils::decimalToBcd(
        time->tm_mon + 1); // The number of months in the range
                           // 0 to 11.PLDM expects range 1 to 12
    *year = (uint16_t)pldm::utils::decimalToBcd(
        time->tm_year + 1900); // The number of years since 1900
}

requester::Coroutine TerminusManager::setDateTime(uint8_t eid)
{
    std::cerr << "Discovery Terminus: " << unsigned(eid)
              << " update date time to terminus." << std::endl;
    uint8_t seconds = 0;
    uint8_t minutes = 0;
    uint8_t hours = 0;
    uint8_t day = 0;
    uint8_t month = 0;
    uint16_t year = 0;

    constexpr auto timeInterface = "xyz.openbmc_project.Time.EpochTime";
    constexpr auto bmcTimePath = "/xyz/openbmc_project/time/bmc";
    EpochTimeUS timeUsec;

    try
    {
        timeUsec = pldm::utils::DBusHandler().getDbusProperty<EpochTimeUS>(
            bmcTimePath, "Elapsed", timeInterface);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        std::cerr << "Error getting time, PATH=" << bmcTimePath
                  << " TIME INTERACE=" << timeInterface << std::endl;
        co_return PLDM_ERROR;
    }

    uint64_t timeSec = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::microseconds(timeUsec))
                           .count();

    epochToBCDTime(timeSec, &seconds, &minutes, &hours, &day, &month, &year);
    std::cerr << "SetDateTime timeUsec=" << timeUsec
              << " seconds=" << unsigned(seconds)
              << " minutes=" << unsigned(minutes)
              << " hours=" << unsigned(hours) << " year=" << year << std::endl;

    std::vector<uint8_t> request(sizeof(pldm_msg_hdr) +
                                 sizeof(struct pldm_set_date_time_req));
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
    auto instanceId = requester.getInstanceId(eid);

    auto rc = encode_set_date_time_req(instanceId, seconds, minutes, hours, day,
                                       month, year, requestMsg,
                                       sizeof(struct pldm_set_date_time_req));
    if (rc != PLDM_SUCCESS)
    {
        requester.markFree(eid, instanceId);
        std::cerr << "Failed to encode_set_date_time_req, rc = " << unsigned(rc)
                  << std::endl;
        co_return PLDM_ERROR;
    }

    const pldm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await SendRecvPldmMsgOverMctp(eid, request, &responseMsg,
                                          &responseLen);
    if (rc)
    {
        std::cerr << "Failed to send sendRecvPldmMsg, EID=" << unsigned(eid)
                  << ", instanceId=" << unsigned(instanceId)
                  << ", type=" << unsigned(PLDM_BIOS)
                  << ", cmd= " << unsigned(PLDM_SET_DATE_TIME)
                  << ", rc=" << unsigned(rc) << std::endl;
        ;
        co_return rc;
    }

    uint8_t completionCode = 0;
    rc = decode_set_date_time_resp(responseMsg, responseLen, &completionCode);

    if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
    {
        std::cerr << "Response Message Error: "
                  << "rc=" << unsigned(rc)
                  << ",completionCode=" << unsigned(completionCode)
                  << std::endl;
        co_return rc;
    }

    std::cerr << "Success SetDateTime to terminus " << unsigned(eid)
              << std::endl;

    co_return completionCode;
}

requester::Coroutine TerminusManager::initMctpTerminus(const MctpInfo& mctpInfo)
{
    mctp_eid_t eid = std::get<0>(mctpInfo);
    tid_t tid = 0;
    auto rc = co_await getTidOverMctp(eid, tid);
    if (rc || tid == PLDM_TID_RESERVED)
    {
        co_return PLDM_ERROR;
    }

    // Assigning a tid. If it has been mapped, mapTid() returns the tid assigned
    // before.
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

    uint64_t supportedTypes = 0;
    rc = co_await getPLDMTypes(tid, supportedTypes);
    if (rc)
    {
        std::cerr << "failed to get PLDM Types\n";
        co_return PLDM_ERROR;
    }

    rc = co_await setDateTime(eid);
    if (rc != PLDM_SUCCESS && rc != PLDM_ERROR_UNSUPPORTED_PLDM_CMD)
    {
        std::cerr << "Failed to setDateTime, rc=" << unsigned(rc) << std::endl;
    }

    rc = co_await setEventReceiver(eid);
    if (rc != PLDM_SUCCESS && rc != PLDM_ERROR_UNSUPPORTED_PLDM_CMD)
    {
        std::cerr << "Failed to setEventReceiver, rc=" << unsigned(rc)
                  << std::endl;
    }

    termini[tid] = std::make_shared<Terminus>(tid, supportedTypes);
    termini[tid]->setMedium(std::get<2>(mctpInfo));
    co_return PLDM_SUCCESS;
}

requester::Coroutine
    TerminusManager::SendRecvPldmMsgOverMctp(mctp_eid_t eid, Request& request,
                                             const pldm_msg** responseMsg,
                                             size_t* responseLen)
{
    auto rc = co_await requester::SendRecvPldmMsg<RequesterHandler>(
        handler, eid, request, responseMsg, responseLen);
    if (rc)
    {
        std::cerr << "sendRecvPldmMsgOverMctp failed. rc="
                  << static_cast<unsigned>(rc) << "\n";
    }
    co_return rc;
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
    rc = co_await SendRecvPldmMsgOverMctp(eid, request, &responseMsg,
                                          &responseLen);
    if (rc)
    {
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

requester::Coroutine
    TerminusManager::SendRecvPldmMsg(tid_t tid, Request& request,
                                     const pldm_msg** responseMsg,
                                     size_t* responseLen)
{
    if (tidPool[tid] &&
        transportLayerTable[tid] == SupportedTransportLayer::MCTP)
    {
        auto mctpInfo = toMctpInfo(tid);
        if (!mctpInfo)
        {
            co_return PLDM_ERROR;
        }

        auto eid = std::get<0>(mctpInfo.value());
        auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
        requestMsg->hdr.instance_id = requester.getInstanceId(eid);
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
