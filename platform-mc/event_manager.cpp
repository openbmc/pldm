#include "event_manager.hpp"

#include "terminus_manager.hpp"

#include <vector>

namespace pldm
{
namespace platform_mc
{

using namespace std::chrono;

void EventManager::startPolling()
{
    if (!pollTimer)
    {
        pollTimer = std::make_unique<phosphor::Timer>(
            event.get(), std::bind_front(&EventManager::doPolling, this));
    }

    if (!pollCriticalEventTimer)
    {
        pollCriticalEventTimer = std::make_unique<phosphor::Timer>(
            event.get(),
            std::bind_front(&EventManager::doFeedCriticalEventCb, this));
    }

    if (!pollTimer->isRunning())
    {
        pollTimer->start(
            duration_cast<std::chrono::milliseconds>(milliseconds(pollingTime)),
            true);
    }

    if (!pollCriticalEventTimer->isRunning())
    {
        pollCriticalEventTimer->start(
            duration_cast<std::chrono::milliseconds>(
                milliseconds(pollingCriticalEventTime)),
            true);
    }
}

void EventManager::stopPolling()
{
    if (pollTimer)
    {
        pollTimer->stop();
    }
    if (pollCriticalEventTimer)
    {
        pollCriticalEventTimer->stop();
    }
}

int EventManager::enqueueCriticalEvent(uint8_t tid, uint16_t eventId)
{
    if (critEventQueue.size() > MAX_QUEUE_SIZE)
        return QUEUE_OVER_FLOW;

    for (auto& i : critEventQueue)
    {
        if (i.first == tid && i.second == eventId)
        {
            return QUEUE_ITEM_EXIST;
        }
    }

#ifdef EVENT_DEBUG
    std::cerr << "\nQUEUING CRIT EVENT_ID " << std::hex << eventId << " TID "
              << std::hex << unsigned(tid) << " at "
              << pldm::utils::getCurrentSystemTime() << "\n";
#endif
    // insert to event queue
    critEventQueue.push_back(std::make_pair(tid, eventId));

    return QUEUE_ADD_SUCCESS;
}

void EventManager::doFeedCriticalEventCb()
{
    this->feedCriticalEventCb();
}

int EventManager::feedCriticalEventCb()
{
    std::pair<uint8_t, uint16_t> eventItem = std::make_pair(0, 0);

    if (isProcessPolling)
        return PLDM_ERROR;

    if (critEventQueue.empty())
    {
        isCritical = false;
        return PLDM_ERROR;
    }

    if (!critEventQueue.empty())
    {
        eventItem = critEventQueue.front();
        critEventQueue.pop_front();
    }

    /* Has Critical Event */
    isCritical = true;
    /* prepare request data */
    reqData.tid = eventItem.first;
    reqData.operationFlag = PLDM_GET_FIRSTPART;
    reqData.dataTransferHandle = eventItem.second;
    reqData.eventIdToAck = eventItem.second;
#ifdef EVENT_DEBUG
    std::cout << "\nHandle Critical EVENT_ID " << std::hex << eventItem.second
              << " TID " << std::hex << unsigned(eventItem.first) << " at "
              << pldm::utils::getCurrentSystemTime() << "\n";
#endif
    if (!pollTimer->isRunning())
    {
        pollTimer->start(
            duration_cast<std::chrono::milliseconds>(milliseconds(pollingTime)),
            true);
    }
    return PLDM_SUCCESS;
}

requester::Coroutine EventManager::doPollingTask()
{
    Request request(sizeof(pldm_msg_hdr) +
                    PLDM_POLL_FOR_PLATFORM_EVENT_MESSAGE_REQ_BYTES);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());

    if (isPolling)
        co_return PLDM_ERROR;

    /* No new event to poll*/
    if (reqData.dataTransferHandle == 0 && reqData.eventIdToAck == 0)
        co_return PLDM_ERROR;

#ifdef EVENT_DEBUG
    std::cout << "\nREQUEST \n"
              << "TransferoperationFlag: " << std::hex
              << (unsigned)reqData.operationFlag << "\n"
              << "eventIdToAck: " << std::hex << reqData.eventIdToAck << "\n"
              << "dataTransferHandle: " << std::hex
              << reqData.dataTransferHandle << "\n";
#endif
    auto rc = encode_poll_for_platform_event_message_req(
        0, 1, reqData.operationFlag, reqData.dataTransferHandle,
        reqData.eventIdToAck, requestMsg,
        PLDM_POLL_FOR_PLATFORM_EVENT_MESSAGE_REQ_BYTES);

    if (rc != PLDM_SUCCESS)
    {
        std::cerr
            << "ERROR: Failed to encode_poll_for_platform_event_message_req(1), rc = "
            << rc << std::endl;
        co_return rc;
    }

    const pldm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await terminusManager.SendRecvPldmMsg(reqData.tid, request,
                                                  &responseMsg, &responseLen);
    if (rc)
    {
        co_return rc;
    }

    processResponseMsg(0, responseMsg, responseLen);

    // flags settings
    isProcessPolling = true;
    isPolling = true;
    responseReceived = false;
    pollReqTimeoutTimer->start(std::chrono::milliseconds(
        (NUMBER_OF_REQUEST_RETRIES + 1) * RESPONSE_TIME_OUT));

    co_return PLDM_ERROR;
}

void EventManager::processResponseMsg(mctp_eid_t /*eid*/,
                                      const pldm_msg* response,
                                      size_t respMsgLen)
{
    uint8_t retCompletionCode;
    uint8_t retTid{};
    uint16_t retEventId;
    uint32_t retNextDataTransferHandle{};
    uint8_t retTransferFlag{};
    uint8_t retEventClass{};
    uint32_t retEventDataSize{};
    uint32_t retEventDataIntegrityChecksum{};

    // announce that data is received
    responseReceived = true;
    isPolling = false;
    pollReqTimeoutTimer->stop();

    std::vector<uint8_t> tmp(respMsgLen, 0);

    auto rc = decode_poll_for_platform_event_message_resp(
        response, respMsgLen, &retCompletionCode, &retTid, &retEventId,
        &retNextDataTransferHandle, &retTransferFlag, &retEventClass,
        &retEventDataSize, tmp.data(), &retEventDataIntegrityChecksum);
    if (rc != PLDM_SUCCESS)
    {
#ifdef EVENT_DEBUG
        std::cerr << "ERROR: Failed to "
                  << "decode_poll_for_platform_event_message_resp, rc = " << rc
                  << std::endl;
#endif
        reset();
        return;
    }

    retEventId &= 0xffff;
    retNextDataTransferHandle &= 0xffffffff;

#ifdef EVENT_DEBUG
    std::cout << "\nRESPONSE: \n"
              << "retTid: " << std::hex << (unsigned)retTid << "\n"
              << "retEventId: " << std::hex << retEventId << "\n"
              << "retNextDataTransferHandle: " << std::hex
              << retNextDataTransferHandle << "\n"
              << "retTransferFlag: " << std::hex << (unsigned)retTransferFlag
              << "\n"
              << "retEventClass: " << std::hex << (unsigned)retEventClass
              << "\n"
              << "retEventDataSize: " << retEventDataSize << "\n"
              << "retEventDataIntegrityChecksum: " << std::hex
              << retEventDataIntegrityChecksum << "\n";
#endif

    if (retEventId == 0x0 || retEventId == 0xffff)
    {
        reset();
        return;
    }

    // drop if response eventId doesn't match with request eventId
    if ((reqData.eventIdToAck != 0x0) && (retEventId != reqData.eventIdToAck))
    {
#ifdef EVENT_DEBUG
        std::cout << "WARNING: RESPONSED EVENT_ID DOESN'T MATCH WITH QUEUING\n"
                  << "Recv EvenID=" << std::hex << retEventId
                  << "Req EvenID=" << std::hex << reqData.eventIdToAck
                  << " TID " << std::hex << unsigned(reqData.tid) << "\n";
#endif
        reset();
        return;
    }

    // found
    int flag = static_cast<int>(retTransferFlag);

    if (flag == PLDM_START) /* Start part */
    {
        recvData.data.insert(recvData.data.begin(), tmp.begin(),
                             tmp.begin() + retEventDataSize);
        recvData.totalSize += retEventDataSize;
        reqData.operationFlag = PLDM_GET_NEXTPART;
        reqData.dataTransferHandle = retNextDataTransferHandle;
        reqData.eventIdToAck = retEventId;
    }
    else if (flag == PLDM_MIDDLE) /* Middle part */
    {
        recvData.data.insert(recvData.data.begin() + reqData.dataTransferHandle,
                             tmp.begin(), tmp.begin() + retEventDataSize);
        recvData.totalSize += retEventDataSize;
        reqData.operationFlag = PLDM_GET_NEXTPART;
        reqData.dataTransferHandle = retNextDataTransferHandle;
        reqData.eventIdToAck = retEventId;
    }
    else if ((flag == PLDM_END) || (flag == PLDM_START_AND_END)) /* End part */
    {
        recvData.data.insert(recvData.data.begin() + reqData.dataTransferHandle,
                             tmp.begin(), tmp.begin() + retEventDataSize);
        recvData.totalSize += retEventDataSize;

        /* eventDataIntegrityChecksum field is only used for multi-part
         * transfer. If single-part, ignore checksum.
         */
        uint32_t checksum = crc32(recvData.data.data(), recvData.data.size());
        if ((flag == PLDM_END) && (checksum != retEventDataIntegrityChecksum))
        {
            std::cerr << "\nchecksum isn't correct chks=" << std::hex
                      << checksum << " eventDataCRC=" << std::hex
                      << retEventDataIntegrityChecksum << "\n ";
        }
        else
        {
            // invoke class handler
            auto it = eventHndls.find(retEventClass);
            if (it != eventHndls.end())
            {
                std::vector<HandlerFunc> handlers =
                    eventHndls.at(retEventClass);
                for (uint8_t i = 0; i < handlers.size(); i++)
                {
                    handlers[i](retTid, retEventClass, retEventId,
                                recvData.data);
                }
            }
        }

        reqData.operationFlag = PLDM_ACKNOWLEDGEMENT_ONLY;
        reqData.dataTransferHandle = 0;
        reqData.eventIdToAck = 0;
    }
#ifdef EVENT_DEBUG
    std::cout << "\nEVENT_ID:" << retEventId
              << " DATA LENGTH:" << recvData.totalSize << " TID " << std::hex
              << unsigned(reqData.tid) << "\n ";
    for (auto it = recvData.data.begin(); it != recvData.data.end(); it++)
    {
        std::cout << std::setfill('0') << std::setw(2) << std::hex
                  << (unsigned)*it << " ";
    }
    std::cout << "\n";
#endif
}

void EventManager::registerEventHandler(uint8_t eventClass,
                                        HandlerFunc function)
{
    auto it = eventHndls.find(eventClass);
    if (it != eventHndls.end())
    {
        eventHndls.at(eventClass).push_back(function);
    }
    else
    {
        std::vector<HandlerFunc> handlers = {function};
        eventHndls.emplace(eventClass, handlers);
    }
}

void EventManager::reset()
{
    isProcessPolling = false;
    isPolling = false;
    responseReceived = false;
    memset(&reqData, 0, sizeof(struct ReqPollInfo));

    recvData.eventClass = 0;
    recvData.totalSize = 0;
    recvData.data.clear();

    pollTimer->stop();
}

void EventManager::handlePldmDbusEventSignal()
{
    pldmEventSignal = std::make_unique<sdbusplus::bus::match_t>(
        pldm::utils::DBusHandler::getBus(),
        sdbusplus::bus::match::rules::type::signal() +
            sdbusplus::bus::match::rules::member("PldmMessagePollEvent") +
            sdbusplus::bus::match::rules::path("/xyz/openbmc_project/pldm") +
            sdbusplus::bus::match::rules::interface(
                "xyz.openbmc_project.PLDM.Event"),
        [&](sdbusplus::message::message& msg) {
            try
            {
                uint8_t msgTID{};
                uint8_t msgEventClass{};
                uint8_t msgFormatVersion{};
                uint16_t msgEventID{};
                uint32_t msgEventDataTransferHandle{};

                // Read the msg and populate each variable
                msg.read(msgTID, msgEventClass, msgFormatVersion, msgEventID,
                         msgEventDataTransferHandle);
#ifdef EVENT_DEBUG
                std::cout << "\n->Coming DBUS Event Signal\n"
                          << "TID: " << std::hex << (unsigned)msgTID << "\n"
                          << "msgEventClass: " << std::hex
                          << (unsigned)msgEventClass << "\n"
                          << "msgFormatVersion: " << std::hex
                          << (unsigned)msgFormatVersion << "\n"
                          << "msgEventID: " << std::hex << msgEventID << "\n"
                          << "msgEventDataTransferHandle: " << std::hex
                          << msgEventDataTransferHandle << "\n";
#endif
                // add the priority
                this->enqueueCriticalEvent(msgTID, msgEventID);
            }
            catch (const std::exception& e)
            {
                std::cerr << "subscribe PldmDbusEventSignal failed\n"
                          << e.what() << std::endl;
            }
        });
}

void EventManager::pollReqTimeoutHdl()
{
    if (!responseReceived)
    {
#ifdef EVENT_DEBUG
        std::cout << "POLL REQ TIMEOUT DROP EVENT_ID \n"
                  << std::hex << reqData.eventIdToAck << " TID " << std::hex
                  << unsigned(reqData.tid) << "\n";
#endif
        // clear cached data
        reset();
    }
}

} // namespace platform_mc
} // namespace pldm
