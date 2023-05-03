#include "platform_manager.hpp"

#include "terminus_manager.hpp"

namespace pldm
{
namespace platform_mc
{

requester::Coroutine PlatformManager::initTerminus()
{
/* DSP0248 section16.9 EventMessageBufferSize Command, the default message
 * buffer size is 256 bytes*/
#define DEFAULT_MESSAGE_BUFFER_SIZE 256

    for (auto& [tid, terminus] : termini)
    {
        if (terminus->initalized)
        {
            continue;
        }

        if (terminus->doesSupport(PLDM_PLATFORM))
        {
            uint16_t terminusMaxBufferSize = terminus->maxBufferSize;
            auto rc = co_await eventMessageBufferSize(
                tid, terminus->maxBufferSize, terminusMaxBufferSize);
            if (rc)
            {
                terminusMaxBufferSize = DEFAULT_MESSAGE_BUFFER_SIZE;
            }

            terminus->maxBufferSize =
                std::min(terminus->maxBufferSize, terminusMaxBufferSize);

            uint8_t synchronyConfiguration = 0;
            uint8_t numberEventClassReturned = 0;
            std::vector<uint8_t> eventClass{};
            rc = co_await eventMessageSupported(
                tid, 1, synchronyConfiguration,
                terminus->synchronyConfigurationSupported,
                numberEventClassReturned, eventClass);
            if (rc)
            {
                terminus->synchronyConfigurationSupported.byte = 0;
            }

            rc = co_await getPDRs(terminus);
            if (!rc)
            {
                terminus->parsePDRs();
            }

            if (terminus->synchronyConfigurationSupported.byte &
                (1 << PLDM_EVENT_MESSAGE_GLOBAL_ENABLE_ASYNC))
            {
                rc = co_await setEventReceiver(
                    tid, PLDM_EVENT_MESSAGE_GLOBAL_ENABLE_ASYNC);
                if (rc)
                {
                    std::cerr << "setEventReceiver failed, rc="
                              << static_cast<unsigned>(rc) << "\n";
                }
            }
        }
        terminus->initalized = true;
    }
    co_return PLDM_SUCCESS;
}

requester::Coroutine
    PlatformManager::getPDRs(std::shared_ptr<Terminus> terminus)
{
    tid_t tid = terminus->getTid();

    uint8_t repositoryState = 0;
    uint32_t recordCount = 0;
    uint32_t repositorySize = 0;
    uint32_t largestRecordSize = 0;
    auto rc = co_await getPDRRepositoryInfo(tid, repositoryState, recordCount,
                                            repositorySize, largestRecordSize);
    if (rc)
    {
        repositoryState = PLDM_AVAILABLE;
        recordCount = std::numeric_limits<uint32_t>::max();
        largestRecordSize = std::numeric_limits<uint32_t>::max();
    }
    else
    {
        if (recordCount < std::numeric_limits<uint32_t>::max())
        {
            recordCount++;
        }
        if (largestRecordSize < std::numeric_limits<uint32_t>::max())
        {
            largestRecordSize++;
        }
    }

    if (repositoryState != PLDM_AVAILABLE)
    {
        co_return PLDM_ERROR_NOT_READY;
    }

    uint32_t recordHndl = 0;
    uint32_t nextRecordHndl = 0;
    uint32_t nextDataTransferHndl = 0;
    uint8_t transferFlag = 0;
    uint16_t responseCnt = 0;
    constexpr uint16_t recvBufSize = 1024;
    std::vector<uint8_t> recvBuf(recvBufSize);
    uint8_t transferCrc = 0;

    terminus->pdrs.clear();
    uint32_t receivedRecordCount = 0;

    do
    {
        rc =
            co_await getPDR(tid, recordHndl, 0, PLDM_GET_FIRSTPART, recvBufSize,
                            0, nextRecordHndl, nextDataTransferHndl,
                            transferFlag, responseCnt, recvBuf, transferCrc);

        if (rc)
        {
            std::cerr << "getPDRs() failed to get fist part of record handle:"
                      << static_cast<unsigned>(recordHndl)
                      << ",tid=" << static_cast<unsigned>(tid)
                      << ",rc=" << static_cast<unsigned>(rc) << "\n";
            terminus->pdrs.clear();
            co_return rc;
        }

        if (transferFlag == PLDM_START || transferFlag == PLDM_START_AND_END)
        {
            // single-part or first-part transfer
            terminus->pdrs.emplace_back(std::vector<uint8_t>(
                recvBuf.begin(), recvBuf.begin() + responseCnt));
            recordHndl = nextRecordHndl;
        }
        else
        {
            // multipart transfer
            uint32_t receivedRecordSize = 0;
            auto pdrHdr = reinterpret_cast<pldm_pdr_hdr*>(recvBuf.data());
            uint16_t recordChgNum = le16toh(pdrHdr->record_change_num);
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
                    std::cerr
                        << "getPDRs() failed to get middle part of record handle:"
                        << static_cast<unsigned>(recordHndl)
                        << ",tid=" << static_cast<unsigned>(tid)
                        << ",rc=" << static_cast<unsigned>(rc) << "\n";
                    terminus->pdrs.clear();
                    co_return rc;
                }

                receivedPdr.insert(receivedPdr.end(), recvBuf.begin(),
                                   recvBuf.begin() + responseCnt);
                receivedRecordSize += responseCnt;

                if (transferFlag == PLDM_END)
                {
                    terminus->pdrs.emplace_back(receivedPdr);
                    recordHndl = nextRecordHndl;
                }
            } while (nextDataTransferHndl != 0 &&
                     receivedRecordSize < largestRecordSize);
        }
        receivedRecordCount++;
    } while (nextRecordHndl != 0 && receivedRecordCount < recordCount);

    co_return PLDM_SUCCESS;
}

requester::Coroutine PlatformManager::getPDR(
    const tid_t tid, const uint32_t recordHndl, const uint32_t dataTransferHndl,
    const uint8_t transferOpFlag, const uint16_t requestCnt,
    const uint16_t recordChgNum, uint32_t& nextRecordHndl,
    uint32_t& nextDataTransferHndl, uint8_t& transferFlag,
    uint16_t& responseCnt, std::vector<uint8_t>& recordData,
    uint8_t& transferCrc)
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
    rc = co_await terminusManager.SendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
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

requester::Coroutine PlatformManager::getPDRRepositoryInfo(
    const tid_t tid, uint8_t& repositoryState, uint32_t& recordCount,
    uint32_t& repositorySize, uint32_t& largestRecordSize)
{
    Request request(sizeof(pldm_msg_hdr) + sizeof(uint8_t));
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
    auto rc = encode_pldm_header_only(PLDM_REQUEST, 0, PLDM_PLATFORM,
                                      PLDM_GET_PDR_REPOSITORY_INFO, requestMsg);
    if (rc)
    {
        co_return rc;
    }

    const pldm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await terminusManager.SendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        co_return rc;
    }

    uint8_t completionCode = 0;
    uint8_t updateTime[PLDM_TIMESTAMP104_SIZE] = {0};
    uint8_t oemUpdateTime[PLDM_TIMESTAMP104_SIZE] = {0};
    uint8_t dataTransferHandleTimeout = 0;

    rc = decode_get_pdr_repository_info_resp(
        responseMsg, responseLen, &completionCode, &repositoryState, updateTime,
        oemUpdateTime, &recordCount, &repositorySize, &largestRecordSize,
        &dataTransferHandleTimeout);
    if (rc)
    {
        co_return rc;
    }
    co_return completionCode;
}

requester::Coroutine PlatformManager::eventMessageBufferSize(
    tid_t tid, uint16_t receiverMaxBufferSize, uint16_t& terminusBufferSize)
{
    Request request(sizeof(pldm_msg_hdr) +
                    PLDM_EVENT_MESSAGE_BUFFER_SIZE_REQ_BYTES);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
    auto rc = encode_event_message_buffer_size_req(0, receiverMaxBufferSize,
                                                   requestMsg);
    if (rc)
    {
        co_return rc;
    }

    const pldm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await terminusManager.SendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        co_return rc;
    }

    uint8_t completionCode;
    rc = decode_event_message_buffer_size_resp(
        responseMsg, responseLen, &completionCode, &terminusBufferSize);
    if (rc)
    {
        co_return rc;
    }
    co_return completionCode;
}

requester::Coroutine PlatformManager::setEventReceiver(
    tid_t tid, pldm_event_message_global_enable eventMessageGlobalEnable)
{
    Request request(sizeof(pldm_msg_hdr) + PLDM_SET_EVENT_RECEIVER_REQ_BYTES);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
    auto rc = encode_set_event_receiver_req(0, eventMessageGlobalEnable, 0x0,
                                            terminusManager.getLocalEid(), 0x0,
                                            requestMsg);
    if (rc)
    {
        co_return rc;
    }

    const pldm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await terminusManager.SendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        co_return rc;
    }

    uint8_t completionCode;
    rc = decode_set_event_receiver_resp(responseMsg, responseLen,
                                        &completionCode);
    if (rc)
    {
        co_return rc;
    }
    co_return completionCode;
}

requester::Coroutine PlatformManager::eventMessageSupported(
    tid_t tid, uint8_t formatVersion, uint8_t& synchronyConfiguration,
    bitfield8_t& synchronyConfigurationSupported,
    uint8_t& numberEventClassReturned, std::vector<uint8_t>& eventClass)
{
    Request request(sizeof(pldm_msg_hdr) +
                    PLDM_EVENT_MESSAGE_SUPPORTED_REQ_BYTES);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
    auto rc = encode_event_message_supported_req(0, formatVersion, requestMsg);
    if (rc)
    {
        co_return rc;
    }

    const pldm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    rc = co_await terminusManager.SendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        co_return rc;
    }

    uint8_t completionCode = 0;
    uint8_t eventClassReturned[std::numeric_limits<uint8_t>::max()];
    uint8_t eventClassCount = std::numeric_limits<uint8_t>::max();

    rc = decode_event_message_supported_resp(
        responseMsg, responseLen, &completionCode, &synchronyConfiguration,
        &synchronyConfigurationSupported, &numberEventClassReturned,
        eventClassReturned, eventClassCount);
    if (rc)
    {
        co_return rc;
    }

    eventClass.insert(eventClass.end(), eventClassReturned,
                      eventClassReturned + numberEventClassReturned);
    co_return completionCode;
}
} // namespace platform_mc
} // namespace pldm
