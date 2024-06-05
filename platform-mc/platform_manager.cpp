#include "platform_manager.hpp"

#include "dbus_impl_fru.hpp"
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
        terminus->initialized = true;

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

        /* Get Fru */
        uint16_t totalTableRecords = 0;
        if (terminus->doesSupportCommand(PLDM_FRU,
                                         PLDM_GET_FRU_RECORD_TABLE_METADATA))
        {
            auto rc = co_await getFRURecordTableMetadata(tid,
                                                         &totalTableRecords);
            if (rc)
            {
                lg2::error(
                    "Failed to get FRU Metadata for terminus {TID}, error {ERROR}",
                    "TID", tid, "ERROR", rc);
            }
            if (!totalTableRecords)
            {
                lg2::error("Number of record table is not correct.");
            }
        }

        if ((totalTableRecords != 0) &&
            terminus->doesSupportCommand(PLDM_FRU, PLDM_GET_FRU_RECORD_TABLE))
        {
            auto rc = co_await getFRURecordTables(tid, totalTableRecords);
            if (rc)
            {
                lg2::error(
                    "Failed to get FRU Records for terminus {TID}, error {ERROR}",
                    "TID", tid, "ERROR", rc);
            }
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
        auto rc = co_await getPDRRepositoryInfo(tid, repositoryState,
                                                recordCount, repositorySize,
                                                largestRecordSize);
        if (rc)
        {
            lg2::error(
                "Failed to get PDR Repository Info for terminus with TID: {TID}, error: {ERROR}",
                "TID", tid, "ERROR", rc);
        }
        else
        {
            recordCount = std::min(recordCount + 1,
                                   std::numeric_limits<uint32_t>::max());
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
        auto rc = co_await getPDR(tid, recordHndl, 0, PLDM_GET_FIRSTPART,
                                  recvBufSize, 0, nextRecordHndl,
                                  nextDataTransferHndl, transferFlag,
                                  responseCnt, recvBuf, transferCrc);

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
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
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
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
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

static std::optional<std::string> fruFieldValuestring(const uint8_t* value,
                                                      const uint8_t& length)
{
    if (!value || !length)
    {
        lg2::error("Fru data to string invalid data.");
        return std::nullopt;
    }

    return std::string(reinterpret_cast<const char*>(value), length);
}

static std::optional<uint32_t> fruFieldParserU32(const uint8_t* value,
                                                 const uint8_t& length)
{
    if (!value || length != 4)
    {
        lg2::error("Fru data to u32 invalid data.");
        return std::nullopt;
    }

    uint32_t ret;
    std::memcpy(&ret, value, length);
    return ret;
}

/** @brief Check if a pointer is go through end of table
 *  @param[in] table - pointer to FRU record table
 *  @param[in] p - pointer to each record of FRU record table
 *  @param[in] tableSize - FRU table size
 */
static bool isTableEnd(const uint8_t* table, const uint8_t* p,
                       size_t& tableSize)
{
    auto offset = p - table;
    return (tableSize - offset) < sizeof(struct pldm_fru_record_data_format);
}

void PlatformManager::createGernalFruDbus(pldm_tid_t tid,
                                          const uint8_t* fruData,
                                          size_t& fruLen)
{
    std::string tidFRUObjPath{};
    std::string fruPath = "/xyz/openbmc_project/pldm/fru";

    if (tid == PLDM_TID_RESERVED || !termini.contains(tid) || !termini[tid])
    {
        lg2::error("Invalid terminus {TID}", "TID", tid);
        return;
    }

    auto tName = termini[tid]->getTerminusName();
    if (!tName.empty())
    {
        tidFRUObjPath = fruPath + "/" + static_cast<std::string>(tName);
    }

    auto& bus = pldm::utils::DBusHandler::getBus();
    std::shared_ptr<pldm::dbus_api::FruReq> fruPtr;
    try
    {
        fruPtr = std::make_shared<pldm::dbus_api::FruReq>(bus, tidFRUObjPath);
        frus.emplace(tid, fruPtr);
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create Fru D-Bus object for Terminus {TID} at path {PATH}",
            "TID", tid, "PATH", tidFRUObjPath);
        return;
    }

    auto ptr = fruData;
    while (!isTableEnd(fruData, ptr, fruLen))
    {
        auto record = reinterpret_cast<const pldm_fru_record_data_format*>(ptr);
        ptr += sizeof(pldm_fru_record_data_format) -
               sizeof(pldm_fru_record_tlv);

        if (record->num_fru_fields == 0)
        {
            lg2::error(
                "Invalid number of fields {NUM} of Record ID Type {TYPE} of terminus {TID}",
                "NUM", record->num_fru_fields, "TYPE", record->record_type,
                "TID", tid);
            return;
        }

        if (record->record_type != PLDM_FRU_RECORD_TYPE_GENERAL)
        {
            lg2::error(
                "Does not support Fru Record ID Type {TYPE} of terminus {TID}",
                "TYPE", record->record_type, "TID", tid);

            for ([[maybe_unused]] const auto& idx :
                 std::views::iota(0, static_cast<int>(record->num_fru_fields)))
            {
                auto tlv = reinterpret_cast<const pldm_fru_record_tlv*>(ptr);
                ptr += sizeof(pldm_fru_record_tlv) - 1 + tlv->length;
            }
            continue;
        }
        /* FRU General record type */
        for ([[maybe_unused]] const auto& idx :
             std::views::iota(0, static_cast<int>(record->num_fru_fields)))
        {
            auto tlv = reinterpret_cast<const pldm_fru_record_tlv*>(ptr);
            std::string fruField{};
            if (tlv->type != PLDM_FRU_FIELD_TYPE_IANA)
            {
                auto strOptional = fruFieldValuestring(tlv->value, tlv->length);
                if (!strOptional)
                {
                    ptr += sizeof(pldm_fru_record_tlv) - 1 + tlv->length;
                    continue;
                }
                fruField = strOptional.value();

                if (fruField.empty())
                {
                    ptr += sizeof(pldm_fru_record_tlv) - 1 + tlv->length;
                    continue;
                }
            }

            switch (tlv->type)
            {
                case PLDM_FRU_FIELD_TYPE_CHASSIS:
                    fruPtr->chassisType(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_MODEL:
                    fruPtr->model(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_PN:
                    fruPtr->pn(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_SN:
                    fruPtr->sn(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_MANUFAC:
                    fruPtr->manufacturer(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_VENDOR:
                    fruPtr->vendor(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_NAME:
                    fruPtr->name(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_SKU:
                    fruPtr->sku(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_VERSION:
                    fruPtr->version(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_ASSET_TAG:
                    fruPtr->assetTag(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_DESC:
                    fruPtr->description(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_EC_LVL:
                    fruPtr->ecLevel(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_OTHER:
                    fruPtr->other(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_IANA:
                    auto iana = fruFieldParserU32(tlv->value, tlv->length);
                    if (!iana)
                    {
                        ptr += sizeof(pldm_fru_record_tlv) - 1 + tlv->length;
                        continue;
                    }
                    fruPtr->iana(iana.value());
                    break;
            }
            ptr += sizeof(pldm_fru_record_tlv) - 1 + tlv->length;
        }
    }
}

exec::task<int> PlatformManager::getFRURecordTableMetadata(pldm_tid_t tid,
                                                           uint16_t* total)
{
    Request request(sizeof(pldm_msg_hdr) +
                    PLDM_GET_FRU_RECORD_TABLE_METADATA_REQ_BYTES);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());

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
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());

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

exec::task<int>
    PlatformManager::getFRURecordTables(pldm_tid_t tid,
                                        const uint16_t& totalTableRecords)
{
    if (!totalTableRecords)
    {
        lg2::error("Number of record table is not correct.");
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
            "Size of Fru Record Data {SIZE} for terminus {TID} is different the responsed size {RSPSIZE}.",
            "SIZE", receivedFru.size(), "RSPSIZE", fruLength);
        co_return PLDM_ERROR_INVALID_LENGTH;
    }

    createGernalFruDbus(tid, receivedFru.data(), fruLength);

    co_return PLDM_SUCCESS;
}

} // namespace platform_mc
} // namespace pldm
