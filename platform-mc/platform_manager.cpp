#include "platform_manager.hpp"

#include "manager.hpp"
#include "terminus_manager.hpp"

#include <phosphor-logging/lg2.hpp>

#include <ranges>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

exec::task<int> PlatformManager::initTerminus()
{
    for (auto& [tid, terminus] : termini)
    {
        if (terminus->initialized)
        {
            continue;
        }

        /* Get Fru */
        uint16_t totalTableRecords = 0;
        if (terminus->doesSupportCommand(PLDM_FRU,
                                         PLDM_GET_FRU_RECORD_TABLE_METADATA))
        {
            auto rc =
                co_await getFRURecordTableMetadata(tid, &totalTableRecords);
            if (rc)
            {
                lg2::error(
                    "Failed to get FRU Metadata for terminus {TID}, error {ERROR}",
                    "TID", tid, "ERROR", rc);
            }
            if (!totalTableRecords)
            {
                lg2::info("Fru record table meta data has 0 records");
            }
        }

        std::vector<uint8_t> fruData{};
        if ((totalTableRecords != 0) &&
            terminus->doesSupportCommand(PLDM_FRU, PLDM_GET_FRU_RECORD_TABLE))
        {
            auto rc =
                co_await getFRURecordTables(tid, totalTableRecords, fruData);
            if (rc)
            {
                lg2::error(
                    "Failed to get Fru Record table for terminus {TID}, error {ERROR}",
                    "TID", tid, "ERROR", rc);
            }
        }

        if (terminus->doesSupportCommand(PLDM_PLATFORM, PLDM_GET_PDR))
        {
            auto rc = co_await getPDRs(terminus);
            if (rc)
            {
                lg2::error(
                    "Failed to fetch PDRs for terminus with TID: {TID}, error: {ERROR}",
                    "TID", tid, "ERROR", rc);
                continue; // Continue to next terminus
            }

            terminus->parseTerminusPDRs();
        }

        /**
         * Need terminus name from PDRs before updating Inventory object with
         * Fru data
         */
        if (fruData.size())
        {
            updateInventoryWithFru(tid, fruData.data(), fruData.size());
        }

        uint16_t terminusMaxBufferSize = terminus->maxBufferSize;
        if (!terminus->doesSupportCommand(PLDM_PLATFORM,
                                          PLDM_EVENT_MESSAGE_BUFFER_SIZE))
        {
            terminusMaxBufferSize = PLDM_PLATFORM_DEFAULT_MESSAGE_BUFFER_SIZE;
        }
        else
        {
            /* Get maxBufferSize use PLDM command eventMessageBufferSize */
            auto rc = co_await eventMessageBufferSize(
                tid, terminus->maxBufferSize, terminusMaxBufferSize);
            if (rc != PLDM_SUCCESS)
            {
                lg2::error(
                    "Failed to get message buffer size for terminus with TID: {TID}, error: {ERROR}",
                    "TID", tid, "ERROR", rc);
                terminusMaxBufferSize =
                    PLDM_PLATFORM_DEFAULT_MESSAGE_BUFFER_SIZE;
            }
        }
        terminus->maxBufferSize =
            std::min(terminus->maxBufferSize, terminusMaxBufferSize);

        auto rc = co_await configEventReceiver(tid);
        if (rc)
        {
            lg2::error(
                "Failed to config event receiver for terminus with TID: {TID}, error: {ERROR}",
                "TID", tid, "ERROR", rc);
        }
        terminus->initialized = true;
        if (manager)
        {
            manager->startSensorPolling(tid);
        }
        else
        {
            lg2::error(
                "Cannot start sensor polling for TID: {TID} because the manager is not initialized.",
                "TID", tid);
        }
    }

    co_return PLDM_SUCCESS;
}

exec::task<int> PlatformManager::configEventReceiver(pldm_tid_t tid)
{
    if (!termini.contains(tid))
    {
        co_return PLDM_ERROR;
    }

    auto& terminus = termini[tid];
    if (!terminus->doesSupportCommand(PLDM_PLATFORM,
                                      PLDM_EVENT_MESSAGE_SUPPORTED))
    {
        terminus->synchronyConfigurationSupported.byte =
            1 << PLDM_EVENT_MESSAGE_GLOBAL_ENABLE_ASYNC_KEEP_ALIVE;
    }
    else
    {
        /**
         *  Get synchronyConfigurationSupported use PLDM command
         *  eventMessageBufferSize
         */
        uint8_t synchronyConfiguration = 0;
        uint8_t numberEventClassReturned = 0;
        std::vector<uint8_t> eventClass{};
        auto rc = co_await eventMessageSupported(
            tid, 1, synchronyConfiguration,
            terminus->synchronyConfigurationSupported, numberEventClassReturned,
            eventClass);
        if (rc != PLDM_SUCCESS)
        {
            lg2::error(
                "Failed to get event message supported for terminus with TID: {TID}, error: {ERROR}",
                "TID", tid, "ERROR", rc);
            terminus->synchronyConfigurationSupported.byte = 0;
        }
    }

    if (!terminus->doesSupportCommand(PLDM_PLATFORM, PLDM_SET_EVENT_RECEIVER))
    {
        lg2::error("Terminus {TID} does not support Event", "TID", tid);
        co_return PLDM_ERROR;
    }

    /**
     *  Set Event receiver base on synchronyConfigurationSupported data
     *  use PLDM command SetEventReceiver
     */
    pldm_event_message_global_enable eventMessageGlobalEnable =
        PLDM_EVENT_MESSAGE_GLOBAL_DISABLE;
    uint16_t heartbeatTimer = 0;

    /* Use PLDM_EVENT_MESSAGE_GLOBAL_ENABLE_ASYNC_KEEP_ALIVE when
     * for eventMessageGlobalEnable when the terminus supports that type
     */
    if (terminus->synchronyConfigurationSupported.byte &
        (1 << PLDM_EVENT_MESSAGE_GLOBAL_ENABLE_ASYNC_KEEP_ALIVE))
    {
        heartbeatTimer = HEARTBEAT_TIMEOUT;
        eventMessageGlobalEnable =
            PLDM_EVENT_MESSAGE_GLOBAL_ENABLE_ASYNC_KEEP_ALIVE;
    }
    /* Use PLDM_EVENT_MESSAGE_GLOBAL_ENABLE_ASYNC when
     * for eventMessageGlobalEnable when the terminus does not support
     * PLDM_EVENT_MESSAGE_GLOBAL_ENABLE_ASYNC_KEEP_ALIVE
     * and supports PLDM_EVENT_MESSAGE_GLOBAL_ENABLE_ASYNC type
     */
    else if (terminus->synchronyConfigurationSupported.byte &
             (1 << PLDM_EVENT_MESSAGE_GLOBAL_ENABLE_ASYNC))
    {
        eventMessageGlobalEnable = PLDM_EVENT_MESSAGE_GLOBAL_ENABLE_ASYNC;
    }
    /* Only use PLDM_EVENT_MESSAGE_GLOBAL_ENABLE_POLLING
     * for eventMessageGlobalEnable when the terminus only supports
     * this type
     */
    else if (terminus->synchronyConfigurationSupported.byte &
             (1 << PLDM_EVENT_MESSAGE_GLOBAL_ENABLE_POLLING))
    {
        eventMessageGlobalEnable = PLDM_EVENT_MESSAGE_GLOBAL_ENABLE_POLLING;
    }

    if (eventMessageGlobalEnable != PLDM_EVENT_MESSAGE_GLOBAL_DISABLE)
    {
        auto rc = co_await setEventReceiver(tid, eventMessageGlobalEnable,
                                            PLDM_TRANSPORT_PROTOCOL_TYPE_MCTP,
                                            heartbeatTimer);
        if (rc != PLDM_SUCCESS)
        {
            lg2::error(
                "Failed to set event receiver for terminus with TID: {TID}, error: {ERROR}",
                "TID", tid, "ERROR", rc);
        }
    }

    co_return PLDM_SUCCESS;
}

exec::task<int> PlatformManager::getPDRs(std::shared_ptr<Terminus> terminus)
{
    pldm_tid_t tid = terminus->getTid();

    /* Setting default values when getPDRRepositoryInfo fails or does not
     * support */
    uint8_t repositoryState = PLDM_AVAILABLE;
    uint32_t recordCount = std::numeric_limits<uint32_t>::max();
    uint32_t repositorySize = 0;
    uint32_t largestRecordSize = std::numeric_limits<uint32_t>::max();
    if (terminus->doesSupportCommand(PLDM_PLATFORM,
                                     PLDM_GET_PDR_REPOSITORY_INFO))
    {
        auto rc =
            co_await getPDRRepositoryInfo(tid, repositoryState, recordCount,
                                          repositorySize, largestRecordSize);
        if (rc)
        {
            lg2::error(
                "Failed to get PDR Repository Info for terminus with TID: {TID}, error: {ERROR}",
                "TID", tid, "ERROR", rc);
        }
        else
        {
            recordCount =
                std::min(recordCount + 1, std::numeric_limits<uint32_t>::max());
            largestRecordSize = std::min(largestRecordSize + 1,
                                         std::numeric_limits<uint32_t>::max());
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
    constexpr uint16_t recvBufSize = PLDM_PLATFORM_GETPDR_MAX_RECORD_BYTES;
    std::vector<uint8_t> recvBuf(recvBufSize);
    uint8_t transferCrc = 0;

    terminus->pdrs.clear();
    uint32_t receivedRecordCount = 0;

    do
    {
        auto rc =
            co_await getPDR(tid, recordHndl, 0, PLDM_GET_FIRSTPART, recvBufSize,
                            0, nextRecordHndl, nextDataTransferHndl,
                            transferFlag, responseCnt, recvBuf, transferCrc);

        if (rc)
        {
            lg2::error(
                "Failed to get PDRs for terminus {TID}, error: {RC}, first part of record handle {RECORD}",
                "TID", tid, "RC", rc, "RECORD", recordHndl);
            terminus->pdrs.clear();
            co_return rc;
        }

        if (transferFlag == PLDM_PLATFORM_TRANSFER_START_AND_END)
        {
            // single-part
            terminus->pdrs.emplace_back(std::vector<uint8_t>(
                recvBuf.begin(), recvBuf.begin() + responseCnt));
            recordHndl = nextRecordHndl;
        }
        else
        {
            // multipart transfer
            uint32_t receivedRecordSize = responseCnt;
            auto pdrHdr = new (recvBuf.data()) pldm_pdr_hdr;
            uint16_t recordChgNum = le16toh(pdrHdr->record_change_num);
            std::vector<uint8_t> receivedPdr(recvBuf.begin(),
                                             recvBuf.begin() + responseCnt);
            do
            {
                rc = co_await getPDR(
                    tid, recordHndl, nextDataTransferHndl, PLDM_GET_NEXTPART,
                    recvBufSize, recordChgNum, nextRecordHndl,
                    nextDataTransferHndl, transferFlag, responseCnt, recvBuf,
                    transferCrc);
                if (rc)
                {
                    lg2::error(
                        "Failed to get PDRs for terminus {TID}, error: {RC}, get middle part of record handle {RECORD}",
                        "TID", tid, "RC", rc, "RECORD", recordHndl);
                    terminus->pdrs.clear();
                    co_return rc;
                }

                receivedPdr.insert(receivedPdr.end(), recvBuf.begin(),
                                   recvBuf.begin() + responseCnt);
                receivedRecordSize += responseCnt;

                if (transferFlag == PLDM_PLATFORM_TRANSFER_END)
                {
                    terminus->pdrs.emplace_back(std::move(receivedPdr));
                    recordHndl = nextRecordHndl;
                }
            } while (nextDataTransferHndl != 0 &&
                     receivedRecordSize < largestRecordSize);
        }
        receivedRecordCount++;
    } while (nextRecordHndl != 0 && receivedRecordCount < recordCount);

    co_return PLDM_SUCCESS;
}

exec::task<int> PlatformManager::getPDR(
    const pldm_tid_t tid, const uint32_t recordHndl,
    const uint32_t dataTransferHndl, const uint8_t transferOpFlag,
    const uint16_t requestCnt, const uint16_t recordChgNum,
    uint32_t& nextRecordHndl, uint32_t& nextDataTransferHndl,
    uint8_t& transferFlag, uint16_t& responseCnt,
    std::vector<uint8_t>& recordData, uint8_t& transferCrc)
{
    Request request(sizeof(pldm_msg_hdr) + PLDM_GET_PDR_REQ_BYTES);
    auto requestMsg = new (request.data()) pldm_msg;
    auto rc = encode_get_pdr_req(0, recordHndl, dataTransferHndl,
                                 transferOpFlag, requestCnt, recordChgNum,
                                 requestMsg, PLDM_GET_PDR_REQ_BYTES);
    if (rc)
    {
        lg2::error(
            "Failed to encode request GetPDR for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    const pldm_msg* responseMsg = nullptr;
    size_t responseLen = 0;
    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        lg2::error(
            "Failed to send GetPDR message for terminus {TID}, error {RC}",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    uint8_t completionCode;
    rc = decode_get_pdr_resp(responseMsg, responseLen, &completionCode,
                             &nextRecordHndl, &nextDataTransferHndl,
                             &transferFlag, &responseCnt, recordData.data(),
                             recordData.size(), &transferCrc);
    if (rc)
    {
        lg2::error(
            "Failed to decode response GetPDR for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    if (completionCode != PLDM_SUCCESS)
    {
        lg2::error("Error : GetPDR for terminus ID {TID}, complete code {CC}.",
                   "TID", tid, "CC", completionCode);
        co_return rc;
    }

    co_return completionCode;
}

exec::task<int> PlatformManager::getPDRRepositoryInfo(
    const pldm_tid_t tid, uint8_t& repositoryState, uint32_t& recordCount,
    uint32_t& repositorySize, uint32_t& largestRecordSize)
{
    Request request(sizeof(pldm_msg_hdr) + sizeof(uint8_t));
    auto requestMsg = new (request.data()) pldm_msg;
    auto rc = encode_pldm_header_only(PLDM_REQUEST, 0, PLDM_PLATFORM,
                                      PLDM_GET_PDR_REPOSITORY_INFO, requestMsg);
    if (rc)
    {
        lg2::error(
            "Failed to encode request GetPDRRepositoryInfo for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    const pldm_msg* responseMsg = nullptr;
    size_t responseLen = 0;
    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        lg2::error(
            "Failed to send GetPDRRepositoryInfo message for terminus {TID}, error {RC}",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    uint8_t completionCode = 0;
    std::array<uint8_t, PLDM_TIMESTAMP104_SIZE> updateTime = {};
    std::array<uint8_t, PLDM_TIMESTAMP104_SIZE> oemUpdateTime = {};
    uint8_t dataTransferHandleTimeout = 0;

    rc = decode_get_pdr_repository_info_resp(
        responseMsg, responseLen, &completionCode, &repositoryState,
        updateTime.data(), oemUpdateTime.data(), &recordCount, &repositorySize,
        &largestRecordSize, &dataTransferHandleTimeout);
    if (rc)
    {
        lg2::error(
            "Failed to decode response GetPDRRepositoryInfo for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    if (completionCode != PLDM_SUCCESS)
    {
        lg2::error(
            "Error : GetPDRRepositoryInfo for terminus ID {TID}, complete code {CC}.",
            "TID", tid, "CC", completionCode);
        co_return rc;
    }

    co_return completionCode;
}

exec::task<int> PlatformManager::eventMessageBufferSize(
    pldm_tid_t tid, uint16_t receiverMaxBufferSize,
    uint16_t& terminusBufferSize)
{
    Request request(
        sizeof(pldm_msg_hdr) + PLDM_EVENT_MESSAGE_BUFFER_SIZE_REQ_BYTES);
    auto requestMsg = new (request.data()) pldm_msg;
    auto rc = encode_event_message_buffer_size_req(0, receiverMaxBufferSize,
                                                   requestMsg);
    if (rc)
    {
        lg2::error(
            "Failed to encode request GetPDRRepositoryInfo for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    const pldm_msg* responseMsg = nullptr;
    size_t responseLen = 0;
    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        lg2::error(
            "Failed to send EventMessageBufferSize message for terminus {TID}, error {RC}",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    uint8_t completionCode;
    rc = decode_event_message_buffer_size_resp(
        responseMsg, responseLen, &completionCode, &terminusBufferSize);
    if (rc)
    {
        lg2::error(
            "Failed to decode response EventMessageBufferSize for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    if (completionCode != PLDM_SUCCESS)
    {
        lg2::error(
            "Error : EventMessageBufferSize for terminus ID {TID}, complete code {CC}.",
            "TID", tid, "CC", completionCode);
        co_return completionCode;
    }

    co_return completionCode;
}

exec::task<int> PlatformManager::setEventReceiver(
    pldm_tid_t tid, pldm_event_message_global_enable eventMessageGlobalEnable,
    pldm_transport_protocol_type protocolType, uint16_t heartbeatTimer)
{
    size_t requestBytes = PLDM_SET_EVENT_RECEIVER_REQ_BYTES;
    /**
     * Ignore heartbeatTimer bytes when eventMessageGlobalEnable is not
     * ENABLE_ASYNC_KEEP_ALIVE
     */
    if (eventMessageGlobalEnable !=
        PLDM_EVENT_MESSAGE_GLOBAL_ENABLE_ASYNC_KEEP_ALIVE)
    {
        requestBytes = requestBytes - sizeof(heartbeatTimer);
    }
    Request request(sizeof(pldm_msg_hdr) + requestBytes);
    auto requestMsg = new (request.data()) pldm_msg;
    auto rc = encode_set_event_receiver_req(
        0, eventMessageGlobalEnable, protocolType,
        terminusManager.getLocalEid(), heartbeatTimer, requestMsg);
    if (rc)
    {
        lg2::error(
            "Failed to encode request SetEventReceiver for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    const pldm_msg* responseMsg = nullptr;
    size_t responseLen = 0;
    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        lg2::error(
            "Failed to send SetEventReceiver message for terminus {TID}, error {RC}",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    uint8_t completionCode;
    rc = decode_set_event_receiver_resp(responseMsg, responseLen,
                                        &completionCode);
    if (rc)
    {
        lg2::error(
            "Failed to decode response SetEventReceiver for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    if (completionCode != PLDM_SUCCESS)
    {
        lg2::error(
            "Error : SetEventReceiver for terminus ID {TID}, complete code {CC}.",
            "TID", tid, "CC", completionCode);
        co_return completionCode;
    }

    co_return completionCode;
}

exec::task<int> PlatformManager::eventMessageSupported(
    pldm_tid_t tid, uint8_t formatVersion, uint8_t& synchronyConfiguration,
    bitfield8_t& synchronyConfigurationSupported,
    uint8_t& numberEventClassReturned, std::vector<uint8_t>& eventClass)
{
    Request request(
        sizeof(pldm_msg_hdr) + PLDM_EVENT_MESSAGE_SUPPORTED_REQ_BYTES);
    auto requestMsg = new (request.data()) pldm_msg;
    auto rc = encode_event_message_supported_req(0, formatVersion, requestMsg);
    if (rc)
    {
        lg2::error(
            "Failed to encode request EventMessageSupported for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    const pldm_msg* responseMsg = nullptr;
    size_t responseLen = 0;
    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        lg2::error(
            "Failed to send EventMessageSupported message for terminus {TID}, error {RC}",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    uint8_t completionCode = 0;
    uint8_t eventClassCount = static_cast<uint8_t>(responseLen) -
                              PLDM_EVENT_MESSAGE_SUPPORTED_MIN_RESP_BYTES;
    eventClass.resize(eventClassCount);

    rc = decode_event_message_supported_resp(
        responseMsg, responseLen, &completionCode, &synchronyConfiguration,
        &synchronyConfigurationSupported, &numberEventClassReturned,
        eventClass.data(), eventClassCount);
    if (rc)
    {
        lg2::error(
            "Failed to decode response EventMessageSupported for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    if (completionCode != PLDM_SUCCESS)
    {
        lg2::error(
            "Error : EventMessageSupported for terminus ID {TID}, complete code {CC}.",
            "TID", tid, "CC", completionCode);
        co_return completionCode;
    }

    co_return completionCode;
}

exec::task<int> PlatformManager::getFRURecordTableMetadata(pldm_tid_t tid,
                                                           uint16_t* total)
{
    Request request(
        sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_METADATA_REQ_BYTES);
    auto requestMsg = new (request.data()) pldm_msg;

    auto rc = encode_get_fru_record_table_metadata_req(
        0, requestMsg, PLDM_GET_FRU_RECORD_TABLE_METADATA_REQ_BYTES);
    if (rc)
    {
        lg2::error(
            "Failed to encode request GetFRURecordTableMetadata for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    const pldm_msg* responseMsg = nullptr;
    size_t responseLen = 0;

    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        lg2::error(
            "Failed to send GetFRURecordTableMetadata message for terminus {TID}, error {RC}",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    uint8_t completionCode = 0;
    if (responseMsg == nullptr || !responseLen)
    {
        lg2::error(
            "No response data for GetFRURecordTableMetadata for terminus {TID}",
            "TID", tid);
        co_return rc;
    }

    uint8_t fru_data_major_version, fru_data_minor_version;
    uint32_t fru_table_maximum_size, fru_table_length;
    uint16_t total_record_set_identifiers;
    uint32_t checksum;
    rc = decode_get_fru_record_table_metadata_resp(
        responseMsg, responseLen, &completionCode, &fru_data_major_version,
        &fru_data_minor_version, &fru_table_maximum_size, &fru_table_length,
        &total_record_set_identifiers, total, &checksum);

    if (rc)
    {
        lg2::error(
            "Failed to decode response GetFRURecordTableMetadata for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    if (completionCode != PLDM_SUCCESS)
    {
        lg2::error(
            "Error : GetFRURecordTableMetadata for terminus ID {TID}, complete code {CC}.",
            "TID", tid, "CC", completionCode);
        co_return rc;
    }

    co_return rc;
}

exec::task<int> PlatformManager::getFRURecordTable(
    pldm_tid_t tid, const uint32_t dataTransferHndl,
    const uint8_t transferOpFlag, uint32_t* nextDataTransferHndl,
    uint8_t* transferFlag, size_t* responseCnt,
    std::vector<uint8_t>& recordData)
{
    Request request(sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_REQ_BYTES);
    auto requestMsg = new (request.data()) pldm_msg;

    auto rc = encode_get_fru_record_table_req(
        0, dataTransferHndl, transferOpFlag, requestMsg,
        PLDM_GET_FRU_RECORD_TABLE_REQ_BYTES);
    if (rc != PLDM_SUCCESS)
    {
        lg2::error(
            "Failed to encode request GetFRURecordTable for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    const pldm_msg* responseMsg = nullptr;
    size_t responseLen = 0;

    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        lg2::error(
            "Failed to send GetFRURecordTable message for terminus {TID}, error {RC}",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    uint8_t completionCode = 0;
    if (responseMsg == nullptr || !responseLen)
    {
        lg2::error("No response data for GetFRURecordTable for terminus {TID}",
                   "TID", tid);
        co_return rc;
    }

    auto responsePtr = reinterpret_cast<const struct pldm_msg*>(responseMsg);
    rc = decode_get_fru_record_table_resp(
        responsePtr, responseLen - sizeof(pldm_msg_hdr), &completionCode,
        nextDataTransferHndl, transferFlag, recordData.data(), responseCnt);

    if (rc)
    {
        lg2::error(
            "Failed to decode response GetFRURecordTable for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    if (completionCode != PLDM_SUCCESS)
    {
        lg2::error(
            "Error : GetFRURecordTable for terminus ID {TID}, complete code {CC}.",
            "TID", tid, "CC", completionCode);
        co_return rc;
    }

    co_return rc;
}

void PlatformManager::updateInventoryWithFru(
    pldm_tid_t tid, const uint8_t* fruData, const size_t fruLen)
{
    if (tid == PLDM_TID_RESERVED || !termini.contains(tid) || !termini[tid])
    {
        lg2::error("Invalid terminus {TID}", "TID", tid);
        return;
    }

    termini[tid]->updateInventoryWithFru(fruData, fruLen);
}

exec::task<int> PlatformManager::getFRURecordTables(
    pldm_tid_t tid, const uint16_t& totalTableRecords,
    std::vector<uint8_t>& fruData)
{
    if (!totalTableRecords)
    {
        lg2::info("Fru record table has 0 records");
        co_return PLDM_ERROR;
    }

    uint32_t dataTransferHndl = 0;
    uint32_t nextDataTransferHndl = 0;
    uint8_t transferFlag = 0;
    uint8_t transferOpFlag = PLDM_GET_FIRSTPART;
    size_t responseCnt = 0;
    std::vector<uint8_t> recvBuf(PLDM_PLATFORM_GETPDR_MAX_RECORD_BYTES);

    size_t fruLength = 0;
    std::vector<uint8_t> receivedFru(0);
    do
    {
        auto rc = co_await getFRURecordTable(
            tid, dataTransferHndl, transferOpFlag, &nextDataTransferHndl,
            &transferFlag, &responseCnt, recvBuf);

        if (rc)
        {
            lg2::error(
                "Failed to get Fru Record Data for terminus {TID}, error: {RC}, first part of data handle {RECORD}",
                "TID", tid, "RC", rc, "RECORD", dataTransferHndl);
            co_return rc;
        }

        receivedFru.insert(receivedFru.end(), recvBuf.begin(),
                           recvBuf.begin() + responseCnt);
        fruLength += responseCnt;
        if (transferFlag == PLDM_PLATFORM_TRANSFER_START_AND_END ||
            transferFlag == PLDM_PLATFORM_TRANSFER_END)
        {
            break;
        }

        // multipart transfer
        dataTransferHndl = nextDataTransferHndl;
        transferOpFlag = PLDM_GET_NEXTPART;

    } while (nextDataTransferHndl != 0);

    if (fruLength != receivedFru.size())
    {
        lg2::error(
            "Size of Fru Record Data {SIZE} for terminus {TID} is different the responded size {RSPSIZE}.",
            "SIZE", receivedFru.size(), "RSPSIZE", fruLength);
        co_return PLDM_ERROR_INVALID_LENGTH;
    }

    fruData = receivedFru;

    co_return PLDM_SUCCESS;
}

} // namespace platform_mc
} // namespace pldm
