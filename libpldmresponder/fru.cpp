#include "fru.hpp"

#include "common/utils.hpp"

#include <libpldm/entity.h>
#include <libpldm/utils.h>
#include <systemd/sd-journal.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>

#include <optional>
#include <set>
#include <stack>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace responder
{

constexpr auto root = "/xyz/openbmc_project/inventory/";

std::optional<pldm_entity> FruImpl::getEntityByObjectPath(
    const dbus::InterfaceMap& intfMaps)
{
    for (const auto& intfMap : intfMaps)
    {
        try
        {
            pldm_entity entity{};
            entity.entity_type = parser.getEntityType(intfMap.first);
            return entity;
        }
        catch (const std::exception&)
        {
            continue;
        }
    }

    return std::nullopt;
}

void FruImpl::updateAssociationTree(const dbus::ObjectValueTree& objects,
                                    const std::string& path)
{
    if (path.find(root) == std::string::npos)
    {
        return;
    }

    std::stack<std::string> tmpObjPaths{};
    tmpObjPaths.emplace(path);

    auto obj = pldm::utils::findParent(path);
    while ((obj + '/') != root)
    {
        tmpObjPaths.emplace(obj);
        obj = pldm::utils::findParent(obj);
    }

    std::stack<std::string> tmpObj = tmpObjPaths;
    while (!tmpObj.empty())
    {
        std::string s = tmpObj.top();
        tmpObj.pop();
    }
    // Update pldm entity to association tree
    std::string prePath = tmpObjPaths.top();
    while (!tmpObjPaths.empty())
    {
        std::string currPath = tmpObjPaths.top();
        tmpObjPaths.pop();

        do
        {
            if (objToEntityNode.contains(currPath))
            {
                pldm_entity node =
                    pldm_entity_extract(objToEntityNode.at(currPath));
                if (pldm_entity_association_tree_find_with_locality(
                        entityTree, &node, false))
                {
                    break;
                }
            }
            else
            {
                if (!objects.contains(currPath))
                {
                    break;
                }

                auto entityPtr = getEntityByObjectPath(objects.at(currPath));
                if (!entityPtr)
                {
                    break;
                }

                pldm_entity entity = *entityPtr;

                for (const auto& it : objToEntityNode)
                {
                    pldm_entity node = pldm_entity_extract(it.second);
                    if (node.entity_type == entity.entity_type)
                    {
                        entity.entity_instance_num =
                            node.entity_instance_num + 1;
                        break;
                    }
                }

                if (currPath == prePath)
                {
                    auto node = pldm_entity_association_tree_add_entity(
                        entityTree, &entity, 0xFFFF, nullptr,
                        PLDM_ENTITY_ASSOCIAION_PHYSICAL, false, true, 0xFFFF);
                    objToEntityNode[currPath] = node;
                }
                else
                {
                    if (objToEntityNode.contains(prePath))
                    {
                        auto node = pldm_entity_association_tree_add_entity(
                            entityTree, &entity, 0xFFFF,
                            objToEntityNode[prePath],
                            PLDM_ENTITY_ASSOCIAION_PHYSICAL, false, true,
                            0xFFFF);
                        objToEntityNode[currPath] = node;
                    }
                }
            }
        } while (0);

        prePath = currPath;
    }
}

void FruImpl::buildFRUTable()
{
    if (isBuilt)
    {
        return;
    }

    fru_parser::DBusLookupInfo dbusInfo;

    try
    {
        dbusInfo = parser.inventoryLookup();
        objects = pldm::utils::DBusHandler::getInventoryObjects<
            pldm::utils::DBusHandler>();
    }
    catch (const std::exception& e)
    {
        error(
            "Failed to build FRU table due to inventory lookup, error - {ERROR}",
            "ERROR", e);
        return;
    }

    auto itemIntfsLookup = std::get<2>(dbusInfo);

    for (const auto& object : objects)
    {
        const auto& interfaces = object.second;
        for (const auto& interface : interfaces)
        {
            if (itemIntfsLookup.contains(interface.first))
            {
                // checking fru present property is available or not.
                if (!pldm::utils::checkForFruPresence(object.first.str))
                {
                    continue;
                }

                // An exception will be thrown by getRecordInfo, if the item
                // D-Bus interface name specified in FRU_Master.json does
                // not have corresponding config jsons
                try
                {
                    updateAssociationTree(objects, object.first.str);
                    pldm_entity entity{};
                    if (objToEntityNode.contains(object.first.str))
                    {
                        pldm_entity_node* node =
                            objToEntityNode.at(object.first.str);

                        entity = pldm_entity_extract(node);
                    }

                    auto recordInfos = parser.getRecordInfo(interface.first);
                    populateRecords(interfaces, recordInfos, entity);

                    associatedEntityMap.emplace(object.first, entity);
                    break;
                }
                catch (const std::exception& e)
                {
                    error(
                        "Config JSONs missing for the item '{INTERFACE}', error - {ERROR}",
                        "INTERFACE", interface.first, "ERROR", e);
                    break;
                }
            }
        }
    }

    int rc = pldm_entity_association_pdr_add(entityTree, pdrRepo, false,
                                             TERMINUS_HANDLE);
    if (rc < 0)
    {
        // pldm_entity_assocation_pdr_add() assert()ed on failure
        error("Failed to add PLDM entity association PDR, response code '{RC}'",
              "RC", rc);
        throw std::runtime_error("Failed to add PLDM entity association PDR");
    }

    // save a copy of bmc's entity association tree
    pldm_entity_association_tree_copy_root(entityTree, bmcEntityTree);

    isBuilt = true;
}
std::string FruImpl::populatefwVersion()
{
    static constexpr auto fwFunctionalObjPath =
        "/xyz/openbmc_project/software/functional";
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::string currentBmcVersion;
    try
    {
        auto method =
            bus.new_method_call(pldm::utils::mapperService, fwFunctionalObjPath,
                                pldm::utils::dbusProperties, "Get");
        method.append("xyz.openbmc_project.Association", "endpoints");
        std::variant<std::vector<std::string>> paths;
        auto reply = bus.call(method, dbusTimeout);
        reply.read(paths);
        auto fwRunningVersion = std::get<std::vector<std::string>>(paths)[0];
        constexpr auto versionIntf = "xyz.openbmc_project.Software.Version";
        auto version = pldm::utils::DBusHandler().getDbusPropertyVariant(
            fwRunningVersion.c_str(), "Version", versionIntf);
        currentBmcVersion = std::get<std::string>(version);
    }
    catch (const std::exception& e)
    {
        error("Failed to make a d-bus call Association, error - {ERROR}",
              "ERROR", e);
        return {};
    }
    return currentBmcVersion;
}
void FruImpl::populateRecords(
    const pldm::responder::dbus::InterfaceMap& interfaces,
    const fru_parser::FruRecordInfos& recordInfos, const pldm_entity& entity)
{
    // recordSetIdentifier for the FRU will be set when the first record gets
    // added for the FRU
    uint16_t recordSetIdentifier = 0;
    auto numRecsCount = numRecs;
    static uint32_t bmc_record_handle = 0;

    for (const auto& [recType, encType, fieldInfos] : recordInfos)
    {
        std::vector<uint8_t> tlvs;
        uint8_t numFRUFields = 0;
        for (const auto& [intf, prop, propType, fieldTypeNum] : fieldInfos)
        {
            try
            {
                pldm::responder::dbus::Value propValue;

                // Assuming that 0 container Id is assigned to the System (as
                // that should be the top most container as per dbus hierarchy)
                if (entity.entity_container_id == 0 && prop == "Version")
                {
                    propValue = populatefwVersion();
                }
                else
                {
                    propValue = interfaces.at(intf).at(prop);
                }
                if (propType == "bytearray")
                {
                    auto byteArray = std::get<std::vector<uint8_t>>(propValue);
                    if (!byteArray.size())
                    {
                        continue;
                    }

                    numFRUFields++;
                    tlvs.emplace_back(fieldTypeNum);
                    tlvs.emplace_back(byteArray.size());
                    std::move(std::begin(byteArray), std::end(byteArray),
                              std::back_inserter(tlvs));
                }
                else if (propType == "string")
                {
                    auto str = std::get<std::string>(propValue);
                    if (!str.size())
                    {
                        continue;
                    }

                    numFRUFields++;
                    tlvs.emplace_back(fieldTypeNum);
                    tlvs.emplace_back(str.size());
                    std::move(std::begin(str), std::end(str),
                              std::back_inserter(tlvs));
                }
            }
            catch (const std::out_of_range&)
            {
                continue;
            }
        }

        if (tlvs.size())
        {
            if (numRecs == numRecsCount)
            {
                recordSetIdentifier = nextRSI();
                bmc_record_handle = nextRecordHandle();
                int rc = pldm_pdr_add_fru_record_set(
                    pdrRepo, TERMINUS_HANDLE, recordSetIdentifier,
                    entity.entity_type, entity.entity_instance_num,
                    entity.entity_container_id, &bmc_record_handle);
                if (rc)
                {
                    // pldm_pdr_add_fru_record_set() assert()ed on failure
                    throw std::runtime_error(
                        "Failed to add PDR FRU record set");
                }
            }
            auto curSize = table.size();
            table.resize(curSize + recHeaderSize + tlvs.size());
            encode_fru_record(table.data(), table.size(), &curSize,
                              recordSetIdentifier, recType, numFRUFields,
                              encType, tlvs.data(), tlvs.size());
            numRecs++;
        }
    }
}

void FruImpl::deleteFRURecord(uint16_t rsi)
{
    std::vector<uint8_t> updatedFruTbl;
    const struct pldm_fru_record_data_format* recordSetSrc =
        reinterpret_cast<const struct pldm_fru_record_data_format*>(
            table.data());

    const struct pldm_fru_record_tlv* tlv;
    size_t pos = 0;

    while ((table.size() > pos) && (recordSetSrc != nullptr))
    {
        size_t recordLen = sizeof(struct pldm_fru_record_data_format) -
                           sizeof(struct pldm_fru_record_tlv);

        tlv = recordSetSrc->tlvs;

        for (const auto& i : std::views::iota(
                 0, static_cast<int>(recordSetSrc->num_fru_fields)))
        {
            size_t len = sizeof(*tlv) - 1 + tlv->length;
            recordLen += len;
            tlv = reinterpret_cast<const struct pldm_fru_record_tlv*>(
                (char*)tlv + len);
        }
        if ((recordSetSrc->record_set_id != htole16(rsi) && rsi != 0))
        {
            std::copy(table.begin() + pos, table.begin() + pos + recordLen,
                      std::back_inserter(updatedFruTbl));
        }
        else
        {
            numRecs--;
        }

        pos += recordLen;
        recordSetSrc =
            reinterpret_cast<const struct pldm_fru_record_data_format*>(tlv);
    }

    table.clear();
    table = std::move(updatedFruTbl);
}

void FruImpl::sendPDRRepositoryChgEventbyPDRHandles(
    std::vector<ChangeEntry>&& pdrRecordHandles,
    std::vector<uint8_t>&& eventDataOps)
{
    uint8_t eventDataFormat = FORMAT_IS_PDR_HANDLES;
    std::vector<uint8_t> numsOfChangeEntries(1);
    std::vector<std::vector<ChangeEntry>> changeEntries(
        numsOfChangeEntries.size());
    for (auto pdrRecordHandle : pdrRecordHandles)
    {
        changeEntries[0].push_back(pdrRecordHandle);
    }
    if (changeEntries.empty())
    {
        return;
    }
    numsOfChangeEntries[0] = changeEntries[0].size();
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
        error(
            "Failed to encode the repository change event data with response code '{RC}'",
            "RC", static_cast<int>(rc));
        return;
    }
    auto instanceId = instanceIdDb.next(mctp_eid);
    std::vector<uint8_t> requestMsg(
        sizeof(pldm_msg_hdr) + PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES +
        actualSize);
    auto request = new (requestMsg.data()) pldm_msg;
    rc = encode_platform_event_message_req(
        instanceId, 1, TERMINUS_ID, PLDM_PDR_REPOSITORY_CHG_EVENT,
        eventDataVec.data(), actualSize, request,
        actualSize + PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES);
    if (rc != PLDM_SUCCESS)
    {
        instanceIdDb.free(mctp_eid, instanceId);
        error(
            "Failed to encode the platform event message with response code '{RC}'",
            "RC" static_cast<unsigned>(rc));
        return;
    }
    auto platformEventMessageResponseHandler = [](mctp_eid_t /*eid*/,
                                                  const pldm_msg* response,
                                                  size_t respMsgLen) {
        if (response == nullptr || !respMsgLen)
        {
            error(
                "Failed to receive response for the PDR repository changed event");
            return;
        }
        uint8_t completionCode{};
        uint8_t status{};
        auto responsePtr = reinterpret_cast<const struct pldm_msg*>(response);
        auto rc = decode_platform_event_message_resp(responsePtr, respMsgLen,
                                                     &completionCode, &status);
        if (rc || completionCode)
        {
            error(
                "Failed to decode platform event message with response code '{RC}' and completion code '{CC}'",
                "RC", static_cast<int>(rc), "CC",
                static_cast<unsigned>(completionCode));
        }
    };
    rc = handler->registerRequest(
        mctp_eid, instanceId, PLDM_PLATFORM, PLDM_PLATFORM_EVENT_MESSAGE,
        std::move(requestMsg), std::move(platformEventMessageResponseHandler));
    if (rc)
    {
        error(
            "Failed to send the PDR repository changed event request after CM");
    }
}

void FruImpl::removeIndividualFRU(const std::string& fruObjPath)
{
    uint16_t rsi = objectPathToRSIMap[fruObjPath];
    if (!rsi)
    {
        info("No Pdrs to delete for the object path {PATH}", "PATH",
             fruObjPath);
        return;
    }
    pldm_entity removeEntity;
    uint16_t terminusHdl{};
    uint16_t entityType{};
    uint16_t entityInsNum{};
    uint16_t containerId{};
    uint32_t updateRecordHdlBmc = 0;
    uint32_t updateRecordHdlHost = 0;
    uint32_t deleteRecordHdl = 0;
    pldm_pdr_fru_record_find_by_rsi(pdrRepo, rsi, &terminusHdl, &entityType,
                                    &entityInsNum, &containerId, false);
    removeEntity.entity_type = entityType;
    removeEntity.entity_instance_num = entityInsNum;
    removeEntity.entity_container_id = containerId;

    uint8_t bmcEventDataOps = -1;
    uint8_t hostEventDataOps = -1;
    auto removeBmcEntityRc =
        pldm_entity_association_pdr_remove_contained_entity(
            pdrRepo, &removeEntity, false, &updateRecordHdlBmc);
    pldm::responder::pdr_utils::PdrEntry pdrEntry;
    uint8_t* pdrData = nullptr;
    auto record =
        pldm_pdr_find_record(pdrRepo, updateRecordHdlBmc, &pdrData,
                             &pdrEntry.size, &pdrEntry.handle.nextRecordHandle);
    if (record)
    {
        error("Found BMC Record {REC}", "REC", updateRecordHdlBmc);
    }
    bmcEventDataOps = record ? PLDM_RECORDS_MODIFIED : PLDM_RECORDS_DELETED;

    auto removeHostEntityRc =
        pldm_entity_association_pdr_remove_contained_entity(
            pdrRepo, &removeEntity, true, &updateRecordHdlHost);
    record =
        pldm_pdr_find_record(pdrRepo, updateRecordHdlHost, &pdrData,
                             &pdrEntry.size, &pdrEntry.handle.nextRecordHandle);
    if (record)
    {
        error("Found Host Record {REC}", "REC", updateRecordHdlHost);
    }
    hostEventDataOps = record ? PLDM_RECORDS_MODIFIED : PLDM_RECORDS_DELETED;

    pldm_pdr_remove_fru_record_set_by_rsi(pdrRepo, rsi, false,
                                          &deleteRecordHdl);

    pldm_entity_association_tree_delete_node(entityTree, removeEntity);
    pldm_entity_association_tree_delete_node(bmcEntityTree, removeEntity);

    objectPathToRSIMap.erase(fruObjPath);
    objToEntityNode.erase(fruObjPath);
    info(
        "Removing Individual FRU [ {FRU_OBJ_PATH} ] with entityid [ {ENTITY_TYPE}, {ENTITY_NUM}, {ENTITY_ID} ]",
        "FRU_OBJ_PATH", fruObjPath, "ENTITY_TYPE",
        static_cast<unsigned>(removeEntity.entity_type), "ENTITY_NUM",
        static_cast<unsigned>(removeEntity.entity_instance_num), "ENTITY_ID",
        static_cast<unsigned>(removeEntity.entity_container_id));
    associatedEntityMap.erase(fruObjPath);

    deleteFRURecord(rsi);

    std::vector<ChangeEntry> handlesTobeDeleted;
    if (deleteRecordHdl != 0)
    {
        handlesTobeDeleted.push_back(deleteRecordHdl);
    }

    std::vector<uint16_t> effecterIDs = findEffecterIds(
        pdrRepo, 0 /*tid*/, removeEntity.entity_type,
        removeEntity.entity_instance_num, removeEntity.entity_container_id);

    for (const auto& ids : effecterIDs)
    {
        auto delEffecterHdl = pldm_delete_by_effecter_id(pdrRepo, ids, false);
        effecterDbusObjMaps.erase(ids);
        if (delEffecterHdl != 0)
        {
            handlesTobeDeleted.push_back(delEffecterHdl);
        }
    }
    std::vector<uint16_t> sensorIDs = findSensorIds(
        pdrRepo, 0 /*tid*/, removeEntity.entity_type,
        removeEntity.entity_instance_num, removeEntity.entity_container_id);

    for (const auto& ids : sensorIDs)
    {
        auto delSensorHdl = pldm_delete_by_sensor_id(pdrRepo, ids, false);
        sensorDbusObjMaps.erase(ids);
        if (delSensorHdl != 0)
        {
            handlesTobeDeleted.push_back(delSensorHdl);
        }
    }

    // need to send both remote and local records. Phyp keeps track of bmc only
    // records
    std::vector<ChangeEntry> handlesTobeModified;
    if (removeBmcEntityRc == 0 && updateRecordHdlBmc != 0)
    {
        (bmcEventDataOps == PLDM_RECORDS_DELETED)
            ? handlesTobeDeleted.push_back(updateRecordHdlBmc)
            : handlesTobeModified.push_back(updateRecordHdlBmc);
    }
    if (removeHostEntityRc == 0 && updateRecordHdlHost != 0)
    {
        (hostEventDataOps == PLDM_RECORDS_DELETED)
            ? handlesTobeDeleted.push_back(updateRecordHdlHost)
            : handlesTobeModified.push_back(updateRecordHdlHost);
    }
    // Adapter PDRs can have deleted records
    if (!handlesTobeDeleted.empty())
    {
        sendPDRRepositoryChgEventbyPDRHandles(
            std::move(handlesTobeDeleted),
            std::vector<uint8_t>(1, PLDM_RECORDS_DELETED));
    }
    if (!handlesTobeModified.empty())
    {
        sendPDRRepositoryChgEventbyPDRHandles(
            std::move(handlesTobeModified),
            (std::vector<uint8_t>(1, PLDM_RECORDS_MODIFIED)));
    }
}

std::vector<uint8_t> FruImpl::tableResize()
{
    std::vector<uint8_t> tempTable;

    if (table.size())
    {
        std::copy(table.begin(), table.end(), std::back_inserter(tempTable));
        padBytes = pldm::utils::getNumPadBytes(table.size());
        tempTable.resize(tempTable.size() + padBytes, 0);
    }
    return tempTable;
}

void FruImpl::getFRUTable(Response& response)
{
    auto hdrSize = response.size();
    std::vector<uint8_t> tempTable;

    if (table.size())
    {
        tempTable = tableResize();
        checksum = pldm_edac_crc32(tempTable.data(), tempTable.size());
    }
    response.resize(hdrSize + tempTable.size() + sizeof(checksum), 0);
    std::copy(tempTable.begin(), tempTable.end(), response.begin() + hdrSize);

    // Copy the checksum to response data
    auto iter = response.begin() + hdrSize + tempTable.size();
    std::copy_n(reinterpret_cast<const uint8_t*>(&checksum), sizeof(checksum),
                iter);
}

void FruImpl::getFRURecordTableMetadata()
{
    std::vector<uint8_t> tempTable;
    if (table.size())
    {
        tempTable = tableResize();
        checksum = pldm_edac_crc32(tempTable.data(), tempTable.size());
    }
}

int FruImpl::getFRURecordByOption(
    std::vector<uint8_t>& fruData, uint16_t /* fruTableHandle */,
    uint16_t recordSetIdentifer, uint8_t recordType, uint8_t fieldType)
{
    using sum = uint32_t;

    // FRU table is built lazily, build if not done.
    buildFRUTable();

    /* 7 is sizeof(checksum,4) + padBytesMax(3)
     * We can not know size of the record table got by options in advance, but
     * it must be less than the source table. So it's safe to use sizeof the
     * source table + 7 as the buffer length
     */
    size_t recordTableSize = table.size() - padBytes + 7;
    fruData.resize(recordTableSize, 0);

    int rc = get_fru_record_by_option(
        table.data(), table.size() - padBytes, fruData.data(), &recordTableSize,
        recordSetIdentifer, recordType, fieldType);

    if (rc != PLDM_SUCCESS || recordTableSize == 0)
    {
        return PLDM_FRU_DATA_STRUCTURE_TABLE_UNAVAILABLE;
    }

    auto pads = pldm::utils::getNumPadBytes(recordTableSize);
    pldm_edac_crc32(fruData.data(), recordTableSize + pads);

    auto iter = fruData.begin() + recordTableSize + pads;
    std::copy_n(reinterpret_cast<const uint8_t*>(&checksum), sizeof(checksum),
                iter);
    fruData.resize(recordTableSize + pads + sizeof(sum));

    return PLDM_SUCCESS;
}

int FruImpl::setFRUTable(const std::vector<uint8_t>& fruData)
{
    auto record =
        reinterpret_cast<const pldm_fru_record_data_format*>(fruData.data());
    if (record)
    {
        if (oemFruHandler && record->record_type == PLDM_FRU_RECORD_TYPE_OEM)
        {
            auto rc = oemFruHandler->processOEMFRUTable(fruData);
            if (!rc)
            {
                return PLDM_SUCCESS;
            }
        }
    }
    return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
}

namespace fru
{
Response Handler::getFRURecordTableMetadata(const pldm_msg* request,
                                            size_t /*payloadLength*/)
{
    // FRU table is built lazily, build if not done.
    buildFRUTable();

    constexpr uint8_t major = 0x01;
    constexpr uint8_t minor = 0x00;
    constexpr uint32_t maxSize = 0xFFFFFFFF;

    Response response(sizeof(pldm_msg_hdr) +
                          PLDM_GET_FRU_RECORD_TABLE_METADATA_RESP_BYTES,
                      0);
    auto responsePtr = new (response.data()) pldm_msg;

    impl.getFRURecordTableMetadata();

    auto rc = encode_get_fru_record_table_metadata_resp(
        request->hdr.instance_id, PLDM_SUCCESS, major, minor, maxSize,
        impl.size(), impl.numRSI(), impl.numRecords(), impl.checkSum(),
        responsePtr);
    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    return response;
}

Response Handler::getFRURecordTable(const pldm_msg* request,
                                    size_t payloadLength)
{
    // FRU table is built lazily, build if not done.
    buildFRUTable();

    if (payloadLength != PLDM_GET_FRU_RECORD_TABLE_REQ_BYTES)
    {
        return ccOnlyResponse(request, PLDM_ERROR_INVALID_LENGTH);
    }

    Response response(
        sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES, 0);
    auto responsePtr = new (response.data()) pldm_msg;

    auto rc =
        encode_get_fru_record_table_resp(request->hdr.instance_id, PLDM_SUCCESS,
                                         0, PLDM_START_AND_END, responsePtr);
    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    impl.getFRUTable(response);

    return response;
}

Response Handler::getFRURecordByOption(const pldm_msg* request,
                                       size_t payloadLength)
{
    if (payloadLength != sizeof(pldm_get_fru_record_by_option_req))
    {
        return ccOnlyResponse(request, PLDM_ERROR_INVALID_LENGTH);
    }

    uint32_t retDataTransferHandle{};
    uint16_t retFruTableHandle{};
    uint16_t retRecordSetIdentifier{};
    uint8_t retRecordType{};
    uint8_t retFieldType{};
    uint8_t retTransferOpFlag{};

    auto rc = decode_get_fru_record_by_option_req(
        request, payloadLength, &retDataTransferHandle, &retFruTableHandle,
        &retRecordSetIdentifier, &retRecordType, &retFieldType,
        &retTransferOpFlag);

    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    std::vector<uint8_t> fruData;
    rc = impl.getFRURecordByOption(fruData, retFruTableHandle,
                                   retRecordSetIdentifier, retRecordType,
                                   retFieldType);
    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    auto respPayloadLength =
        PLDM_GET_FRU_RECORD_BY_OPTION_MIN_RESP_BYTES + fruData.size();
    Response response(sizeof(pldm_msg_hdr) + respPayloadLength, 0);
    auto responsePtr = new (response.data()) pldm_msg;

    rc = encode_get_fru_record_by_option_resp(
        request->hdr.instance_id, PLDM_SUCCESS, 0, PLDM_START_AND_END,
        fruData.data(), fruData.size(), responsePtr, respPayloadLength);

    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    return response;
}

Response Handler::setFRURecordTable(const pldm_msg* request,
                                    size_t payloadLength)
{
    uint32_t transferHandle{};
    uint8_t transferOpFlag{};
    struct variable_field fruData;

    auto rc = decode_set_fru_record_table_req(
        request, payloadLength, &transferHandle, &transferOpFlag, &fruData);

    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    Table table(fruData.ptr, fruData.ptr + fruData.length);
    rc = impl.setFRUTable(table);
    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    Response response(
        sizeof(pldm_msg_hdr) + PLDM_SET_FRU_RECORD_TABLE_RESP_BYTES);
    struct pldm_msg* responsePtr = new (response.data()) pldm_msg;

    rc = encode_set_fru_record_table_resp(
        request->hdr.instance_id, PLDM_SUCCESS, 0 /* nextDataTransferHandle */,
        response.size() - sizeof(pldm_msg_hdr), responsePtr);

    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    return response;
}

} // namespace fru

} // namespace responder

} // namespace pldm
