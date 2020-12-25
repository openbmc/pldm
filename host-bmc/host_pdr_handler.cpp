#include "config.h"

#include "host_pdr_handler.hpp"

#include "libpldm/fru.h"
#include "libpldm/requester/pldm.h"
#include "oem/ibm/libpldm/fru.h"

#include "custom_dbus.hpp"

#include <assert.h>

#include <nlohmann/json.hpp>

#include <fstream>

namespace pldm
{

using namespace pldm::utils;
using namespace sdbusplus::bus::match::rules;
using Json = nlohmann::json;
namespace fs = std::filesystem;
using namespace pldm::dbus;
constexpr auto fruJson = "host_frus.json";
const Json emptyJson{};
const std::vector<Json> emptyJsonList{};

HostPDRHandler::HostPDRHandler(
    int mctp_fd, uint8_t mctp_eid, sdeventplus::Event& event, pldm_pdr* repo,
    const std::string& eventsJsonsDir, pldm_entity_association_tree* entityTree,
    pldm_entity_association_tree* bmcEntityTree, Requester& requester,
    pldm::requester::Handler<pldm::requester::Request>& handler, bool verbose) :
    mctp_fd(mctp_fd),
    mctp_eid(mctp_eid), event(event), repo(repo),
    stateSensorHandler(eventsJsonsDir), entityTree(entityTree),
    bmcEntityTree(bmcEntityTree), requester(requester), handler(handler),
    verbose(verbose)
{
    fs::path hostFruJson(fs::path(HOST_JSONS_DIR) / fruJson);
    if (fs::exists(hostFruJson))
    {
        // Note parent entities for entities sent down by the host firmware.
        // This will enable a merge of entity associations.
        try
        {
            std::ifstream jsonFile(hostFruJson);
            auto data = Json::parse(jsonFile, nullptr, false);
            if (data.is_discarded())
            {
                std::cerr << "Parsing Host FRU json file failed" << std::endl;
            }
            else
            {
                auto entities = data.value("entities", emptyJsonList);
                for (auto& entity : entities)
                {
                    EntityType entityType = entity.value("entity_type", 0);
                    auto parent = entity.value("parent", emptyJson);
                    pldm_entity p{};
                    p.entity_type = parent.value("entity_type", 0);
                    p.entity_instance_num = parent.value("entity_instance", 0);
                    parents.emplace(entityType, std::move(p));
                }
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Parsing Host FRU json file failed, exception = "
                      << e.what() << std::endl;
        }
    }

    hostOffMatch = std::make_unique<sdbusplus::bus::match::match>(
        pldm::utils::DBusHandler::getBus(),
        propertiesChanged("/xyz/openbmc_project/state/host0",
                          "xyz.openbmc_project.State.Host"),
        [this, repo, entityTree,
         bmcEntityTree](sdbusplus::message::message& msg) {
            DbusChangedProps props{};
            std::string intf;
            msg.read(intf, props);
            const auto itr = props.find("CurrentHostState");
            if (itr != props.end())
            {
                PropertyValue value = itr->second;
                auto propVal = std::get<std::string>(value);
                if (propVal == "xyz.openbmc_project.State.Host.HostState.Off")
                {
                    pldm_pdr_remove_remote_pdrs(repo);
                    pldm_entity_association_tree_destroy_root(entityTree);
                    pldm_entity_association_tree_copy_root(bmcEntityTree,
                                                           entityTree);
                    this->sensorMap.clear();
                }
            }
        });
}

void HostPDRHandler::fetchPDR(PDRRecordHandles&& recordHandles)
{
    pdrRecordHandles.clear();
    pdrRecordHandles = std::move(recordHandles);

    // Defer the actual fetch of PDRs from the host (by queuing the call on the
    // main event loop). That way, we can respond to the platform event msg from
    // the host firmware.
    pdrFetchEvent = std::make_unique<sdeventplus::source::Defer>(
        event, std::bind(std::mem_fn(&HostPDRHandler::_fetchPDR), this,
                         std::placeholders::_1));
}

void HostPDRHandler::_fetchPDR(sdeventplus::source::EventBase& /*source*/)
{
    pdrFetchEvent.reset();

    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                    PLDM_GET_PDR_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    bool merged = false;
    PDRList stateSensorPDRs{};
    PDRList fruRecordSetPDRs{};
    TLPDRMap tlpdrInfo{};

    uint32_t nextRecordHandle{};
    uint32_t recordHandle{};
    bool isFormatRecHandles = false;
    if (!pdrRecordHandles.empty())
    {
        recordHandle = pdrRecordHandles.front();
        pdrRecordHandles.pop_front();
        isFormatRecHandles = true;
    }

    do
    {
        auto instanceId = requester.getInstanceId(mctp_eid);

        auto rc =
            encode_get_pdr_req(instanceId, recordHandle, 0, PLDM_GET_FIRSTPART,
                               UINT16_MAX, 0, request, PLDM_GET_PDR_REQ_BYTES);
        if (rc != PLDM_SUCCESS)
        {
            requester.markFree(mctp_eid, instanceId);
            std::cerr << "Failed to encode_get_pdr_req, rc = " << rc
                      << std::endl;
            return;
        }
        if (verbose)
        {
            std::cout << "Sending Msg:" << std::endl;
            printBuffer(requestMsg, verbose);
        }

        uint8_t* responseMsg = nullptr;
        size_t responseMsgSize{};
        auto requesterRc =
            pldm_send_recv(mctp_eid, mctp_fd, requestMsg.data(),
                           requestMsg.size(), &responseMsg, &responseMsgSize);
        std::unique_ptr<uint8_t, decltype(std::free)*> responseMsgPtr{
            responseMsg, std::free};
        requester.markFree(mctp_eid, instanceId);
        if (requesterRc != PLDM_REQUESTER_SUCCESS)
        {
            std::cerr << "Failed to send msg to fetch pdrs, rc = "
                      << requesterRc << std::endl;
            return;
        }

        uint8_t completionCode{};
        uint32_t nextDataTransferHandle{};
        uint8_t transferFlag{};
        uint16_t respCount{};
        uint8_t transferCRC{};
        auto responsePtr =
            reinterpret_cast<struct pldm_msg*>(responseMsgPtr.get());
        rc = decode_get_pdr_resp(
            responsePtr, responseMsgSize - sizeof(pldm_msg_hdr),
            &completionCode, &nextRecordHandle, &nextDataTransferHandle,
            &transferFlag, &respCount, nullptr, 0, &transferCRC);

        std::vector<uint8_t> responsePDRMsg;
        responsePDRMsg.resize(responseMsgSize);
        memcpy(responsePDRMsg.data(), responsePtr, responsePDRMsg.size());
        if (verbose)
        {
            std::cout << "Receiving Msg:" << std::endl;
            printBuffer(responsePDRMsg, verbose);
        }
        if (rc != PLDM_SUCCESS)
        {
            std::cerr << "Failed to decode_get_pdr_resp, rc = " << rc
                      << std::endl;
        }
        else
        {
            std::vector<uint8_t> pdr(respCount, 0);
            rc = decode_get_pdr_resp(
                responsePtr, responseMsgSize - sizeof(pldm_msg_hdr),
                &completionCode, &nextRecordHandle, &nextDataTransferHandle,
                &transferFlag, &respCount, pdr.data(), respCount, &transferCRC);
            if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
            {
                std::cerr << "Failed to decode_get_pdr_resp: "
                          << "rc=" << rc
                          << ", cc=" << static_cast<unsigned>(completionCode)
                          << std::endl;
            }
            else
            {
                // Process the PDR host firmware sent us. The most common action
                // is to add the PDR to the the BMC's PDR repo.
                auto pdrHdr = reinterpret_cast<pldm_pdr_hdr*>(pdr.data());
                if (pdrHdr->type == PLDM_PDR_ENTITY_ASSOCIATION)
                {
                    mergeEntityAssociations(pdr);
                    merged = true;
                }
                else
                {
                    if (pdrHdr->type == PLDM_TERMINUS_LOCATOR_PDR)
                    {
                        auto tlpdr =
                            reinterpret_cast<const pldm_terminus_locator_pdr*>(
                                pdr.data());
                        tlpdrInfo.emplace(
                            static_cast<pdr::TerminusHandle>(
                                tlpdr->terminus_handle),
                            static_cast<pdr::TerminusID>(tlpdr->tid));
                    }
                    else if (pdrHdr->type == PLDM_STATE_SENSOR_PDR)
                    {
                        stateSensorPDRs.emplace_back(pdr);
                    }
                    else if (pdrHdr->type == PLDM_PDR_FRU_RECORD_SET)
                    {
                        fruRecordSetPDRs.emplace_back(pdr);
                    }

                    pldm_pdr_add(repo, pdr.data(), respCount, 0, true);
                }
            }

            recordHandle = nextRecordHandle;
            if (!pdrRecordHandles.empty())
            {
                recordHandle = pdrRecordHandles.front();
                pdrRecordHandles.pop_front();
            }
            else if (isFormatRecHandles)
            {
                break;
            }
        }
    } while (recordHandle);

    updateEntityAssociation(entityAssociations, entityTree, objPathMap);
    entityAssociations.clear();

    parseStateSensorPDRs(stateSensorPDRs, tlpdrInfo);
    parseFruRecordSetPDRs(fruRecordSetPDRs);

    if (merged)
    {
        // We have merged host's entity association PDRs with our own. Send an
        // event to the host firmware to indicate the same.
        sendPDRRepositoryChgEvent(
            std::move(std::vector<uint8_t>(1, PLDM_PDR_ENTITY_ASSOCIATION)),
            FORMAT_IS_PDR_HANDLES);
    }
}

int HostPDRHandler::handleStateSensorEvent(const StateSensorEntry& entry,
                                           pdr::EventState state)
{
    auto rc = stateSensorHandler.eventAction(entry, state);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Failed to fetch and update D-bus property, rc = " << rc
                  << std::endl;
        return rc;
    }
    return PLDM_SUCCESS;
}

void HostPDRHandler::mergeEntityAssociations(const std::vector<uint8_t>& pdr)
{
    size_t numEntities{};
    pldm_entity* entities = nullptr;
    bool merged = false;
    auto entityPdr = reinterpret_cast<pldm_pdr_entity_association*>(
        const_cast<uint8_t*>(pdr.data()) + sizeof(pldm_pdr_hdr));

    pldm_entity_association_pdr_extract(pdr.data(), pdr.size(), &numEntities,
                                        &entities);
    if (numEntities > 0)
    {
        auto pNode =
            pldm_entity_association_tree_find(entityTree, &entities[0]);
        if (!pNode)
        {
            return;
        }

        Entities entityAssoc;
        entityAssoc.push_back(pldm_entity_extract(pNode));
        for (size_t i = 1; i < numEntities; ++i)
        {
            auto cNode =
                pldm_entity_association_tree_find(entityTree, &entities[i]);

            pldm_entity node = pldm_entity_extract(pNode);
            pldm_entity parent;
            if (cNode)
            {
                parent = pldm_entity_get_parent(cNode);
            }
            if (!cNode || parent.entity_type != node.entity_type ||
                parent.entity_instance_num != node.entity_instance_num ||
                parent.entity_container_id != node.entity_container_id)
            {
                cNode = pldm_entity_association_tree_add(
                    entityTree, &entities[i], pNode,
                    entityPdr->association_type);
                merged = true;
            }
            entityAssoc.push_back(pldm_entity_extract(cNode));
        }

        if (merged)
        {
            entityAssociations.push_back(entityAssoc);
        }
    }
    free(entities);

    if (merged)
    {
        // Update our PDR repo with the merged entity association PDRs
        pldm_entity_association_pdr_add(entityTree, repo, true);
    }
}

void HostPDRHandler::sendPDRRepositoryChgEvent(std::vector<uint8_t>&& pdrTypes,
                                               uint8_t eventDataFormat)
{
    assert(eventDataFormat == FORMAT_IS_PDR_HANDLES);

    // Extract from the PDR repo record handles of PDRs we want the host
    // to pull up.
    std::vector<uint8_t> eventDataOps{PLDM_RECORDS_ADDED};
    std::vector<uint8_t> numsOfChangeEntries(1);
    std::vector<std::vector<ChangeEntry>> changeEntries(
        numsOfChangeEntries.size());
    for (auto pdrType : pdrTypes)
    {
        const pldm_pdr_record* record{};
        do
        {
            record = pldm_pdr_find_record_by_type(repo, pdrType, record,
                                                  nullptr, nullptr);
            if (record && pldm_pdr_record_is_remote(record))
            {
                changeEntries[0].push_back(
                    pldm_pdr_get_record_handle(repo, record));
            }
        } while (record);
    }
    if (changeEntries.empty())
    {
        return;
    }
    numsOfChangeEntries[0] = changeEntries[0].size();

    // Encode PLDM platform event msg to indicate a PDR repo change.
    size_t maxSize = PLDM_PDR_REPOSITORY_CHG_EVENT_MIN_LENGTH +
                     PLDM_PDR_REPOSITORY_CHANGE_RECORD_MIN_LENGTH +
                     changeEntries[0].size() * sizeof(uint32_t);
    std::vector<uint8_t> eventDataVec{};
    eventDataVec.resize(maxSize);
    auto eventData =
        reinterpret_cast<struct pldm_pdr_repository_chg_event_data*>(
            eventDataVec.data());
    size_t actualSize{};
    auto firstEntry = changeEntries[0].data();
    auto rc = encode_pldm_pdr_repository_chg_event_data(
        eventDataFormat, 1, eventDataOps.data(), numsOfChangeEntries.data(),
        &firstEntry, eventData, &actualSize, maxSize);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr
            << "Failed to encode_pldm_pdr_repository_chg_event_data, rc = "
            << rc << std::endl;
        return;
    }
    auto instanceId = requester.getInstanceId(mctp_eid);
    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                    PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES +
                                    actualSize);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    rc = encode_platform_event_message_req(
        instanceId, 1, 0, PLDM_PDR_REPOSITORY_CHG_EVENT, eventDataVec.data(),
        actualSize, request,
        actualSize + PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES);
    if (rc != PLDM_SUCCESS)
    {
        requester.markFree(mctp_eid, instanceId);
        std::cerr << "Failed to encode_platform_event_message_req, rc = " << rc
                  << std::endl;
        return;
    }

    auto platformEventMessageResponseHandler = [](mctp_eid_t /*eid*/,
                                                  const pldm_msg* response,
                                                  size_t respMsgLen) {
        if (response == nullptr || !respMsgLen)
        {
            std::cerr << "Failed to receive response for the PDR repository "
                         "changed event"
                      << "\n";
            return;
        }

        uint8_t completionCode{};
        uint8_t status{};
        auto responsePtr = reinterpret_cast<const struct pldm_msg*>(response);
        auto rc = decode_platform_event_message_resp(
            responsePtr, respMsgLen - sizeof(pldm_msg_hdr), &completionCode,
            &status);
        if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
        {
            std::cerr << "Failed to decode_platform_event_message_resp: "
                      << "rc=" << rc
                      << ", cc=" << static_cast<unsigned>(completionCode)
                      << std::endl;
        }
    };

    rc = handler.registerRequest(
        mctp_eid, instanceId, PLDM_PLATFORM, PLDM_PDR_REPOSITORY_CHG_EVENT,
        std::move(requestMsg), std::move(platformEventMessageResponseHandler));
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Failed to send the PDR repository changed event request"
                  << "\n";
    }
}

void HostPDRHandler::parseStateSensorPDRs(const PDRList& stateSensorPDRs,
                                          const TLPDRMap& tlpdrInfo)
{
    for (const auto& pdr : stateSensorPDRs)
    {
        SensorEntry sensorEntry{};
        const auto& [terminusHandle, sensorID, sensorInfo] =
            responder::pdr_utils::parseStateSensorPDR(pdr);
        sensorEntry.sensorID = sensorID;
        try
        {
            sensorEntry.terminusID = tlpdrInfo.at(terminusHandle);
        }
        // If there is no mapping for terminusHandle assign the reserved TID
        // value of 0xFF to indicate that.
        catch (const std::out_of_range& e)
        {
            sensorEntry.terminusID = PLDM_TID_RESERVED;
        }
        sensorMap.emplace(sensorEntry, std::move(sensorInfo));
    }
}

uint16_t HostPDRHandler::getFRURecordTableMetadataByHost()
{
    auto instanceId = requester.getInstanceId(mctp_eid);
    std::vector<uint8_t> requestMsg(
        sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_METADATA_REQ_BYTES);

    // GetFruRecordTableMetadata
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_get_fru_record_table_metadata_req(
        instanceId, request, requestMsg.size() - sizeof(pldm_msg_hdr));
    if (rc != PLDM_SUCCESS)
    {
        requester.markFree(mctp_eid, instanceId);
        std::cerr << "Failed to encode_get_fru_record_table_metadata_req, rc = "
                  << rc << std::endl;
        return 0;
    }

    uint8_t* responseMsg = nullptr;
    size_t responseMsgSize{};
    auto requesterRc =
        pldm_send_recv(mctp_eid, mctp_fd, requestMsg.data(), requestMsg.size(),
                       &responseMsg, &responseMsgSize);
    std::unique_ptr<uint8_t, decltype(std::free)*> responseMsgPtr{responseMsg,
                                                                  std::free};
    requester.markFree(mctp_eid, instanceId);
    if (requesterRc != PLDM_REQUESTER_SUCCESS)
    {
        std::cerr
            << "Failed to send msg to get fru record table metadata, rc = "
            << requesterRc << std::endl;
        return 0;
    }

    uint8_t cc = 0;
    uint8_t fru_data_major_version, fru_data_minor_version;
    uint32_t fru_table_maximum_size, fru_table_length;
    uint16_t total_record_set_identifiers, total_table_records;
    uint32_t checksum;
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsgPtr.get());
    rc = decode_get_fru_record_table_metadata_resp(
        responsePtr, responseMsgSize - sizeof(pldm_msg_hdr), &cc,
        &fru_data_major_version, &fru_data_minor_version,
        &fru_table_maximum_size, &fru_table_length,
        &total_record_set_identifiers, &total_table_records, &checksum);
    if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
    {
        std::cerr << "Faile to decode get fru record table metadata resp, "
                     "Message Error: "
                  << "rc=" << rc << ",cc=" << (int)cc << std::endl;
        return 0;
    }

    return total_table_records;
}

void HostPDRHandler::getFRURecordTableByHost()
{
    fruRecordData.clear();

    // send the getFruRecordTableMetadata command
    uint16_t total_table_records = getFRURecordTableMetadataByHost();
    if (!total_table_records)
    {
        std::cerr << "Failed to get fru record table." << std::endl;
        return;
    }

    auto instanceId = requester.getInstanceId(mctp_eid);
    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                    PLDM_GET_FRU_RECORD_TABLE_REQ_BYTES);

    // send the getFruRecordTable command
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_get_fru_record_table_req(
        instanceId, 0, PLDM_GET_FIRSTPART, request,
        requestMsg.size() - sizeof(pldm_msg_hdr));
    if (rc != PLDM_SUCCESS)
    {
        requester.markFree(mctp_eid, instanceId);
        std::cerr << "Failed to encode_get_fru_record_table_req, rc = " << rc
                  << std::endl;
        return;
    }

    uint8_t* responseMsg = nullptr;
    size_t responseMsgSize{};
    auto requesterRc =
        pldm_send_recv(mctp_eid, mctp_fd, requestMsg.data(), requestMsg.size(),
                       &responseMsg, &responseMsgSize);
    std::unique_ptr<uint8_t, decltype(std::free)*> responseMsgPtr{responseMsg,
                                                                  std::free};
    requester.markFree(mctp_eid, instanceId);
    if (requesterRc != PLDM_REQUESTER_SUCCESS)
    {
        std::cerr << "Failed to send msg to get fru record table, rc = "
                  << requesterRc << std::endl;
        return;
    }

    uint8_t cc = 0;
    uint32_t next_data_transfer_handle = 0;
    uint8_t transfer_flag = 0;
    size_t fru_record_table_length = 0;
    std::vector<uint8_t> fru_record_table_data(responseMsgSize -
                                               sizeof(pldm_msg_hdr));
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsgPtr.get());
    rc = decode_get_fru_record_table_resp(
        responsePtr, responseMsgSize - sizeof(pldm_msg_hdr), &cc,
        &next_data_transfer_handle, &transfer_flag,
        fru_record_table_data.data(), &fru_record_table_length);

    if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
    {
        std::cerr
            << "Faile to decode get fru record table resp, Message Error: "
            << "rc=" << rc << ",cc=" << (int)cc << std::endl;
        return;
    }

    fruRecordData = responder::pdr_utils::parseFruRecordTable(
        fru_record_table_data.data(), fru_record_table_length);

    if (total_table_records != fruRecordData.size())
    {
        fruRecordData.clear();

        std::cerr << "failed to parse fru recrod data format.\n";
        return;
    }
}

uint16_t HostPDRHandler::getRSI(const PDRList& fruRecordSetPDRs,
                                const pldm_entity& entity)
{
    uint16_t fruRSI = 0;

    for (const auto& pdr : fruRecordSetPDRs)
    {
        auto fruPdr = reinterpret_cast<const pldm_pdr_fru_record_set*>(
            const_cast<uint8_t*>(pdr.data()) + sizeof(pldm_pdr_hdr));

        if (fruPdr->entity_type == entity.entity_type &&
            fruPdr->entity_instance_num == entity.entity_instance_num)
        {
            fruRSI = fruPdr->fru_rsi;
            break;
        }
    }

    return fruRSI;
}

void HostPDRHandler::parseFruRecordSetPDRs(const PDRList& fruRecordSetPDRs)
{
    getFRURecordTableByHost();

    for (auto& entity : objPathMap)
    {
        auto fruRSI = getRSI(fruRecordSetPDRs, entity.second);

        for (auto& data : fruRecordData)
        {
            if (fruRSI != data.fruRSI)
            {
                continue;
            }

            if (data.fruRecType == PLDM_FRU_RECORD_TYPE_OEM)
            {
                for (auto& tlv : data.fruTLV)
                {
                    if (tlv.fruFieldType ==
                        PLDM_OEM_FRU_FIELD_TYPE_LOCATION_CODE)
                    {
                        CustomDBus::getCustomDBus().setLocationCode(
                            entity.first,
                            std::string(reinterpret_cast<const char*>(
                                            tlv.fruFieldValue.data()),
                                        tlv.fruFieldLen));
                    }
                }
            }
        }
    }
}

} // namespace pldm
