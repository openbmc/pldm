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
    size_t pos = 0;

    while (pos < table.size())
    {
        // Ensure enough space for the record header
        if ((table.size() - pos) < sizeof(struct pldm_fru_record_data_format))
        {
            // Log or handle corrupt/truncated record
            error("Error: Incomplete FRU record header");
            return;
        }

        auto recordSetSrc =
            reinterpret_cast<const struct pldm_fru_record_data_format*>(
                &table[pos]);

        size_t recordLen = sizeof(struct pldm_fru_record_data_format) -
                           sizeof(struct pldm_fru_record_tlv);

        const struct pldm_fru_record_tlv* tlv = recordSetSrc->tlvs;

        for (uint8_t i = 0; i < recordSetSrc->num_fru_fields; ++i)
        {
            if ((table.size() - pos) < (recordLen + sizeof(*tlv)))
            {
                error("Error: Incomplete TLV header");
                return;
            }

            size_t len = sizeof(*tlv) - 1 + tlv->length;

            if ((table.size() - pos) < (recordLen + len))
            {
                error("Error: Incomplete TLV data");
                return;
            }

            recordLen += len;

            // Advance to next tlv
            tlv = reinterpret_cast<const struct pldm_fru_record_tlv*>(
                reinterpret_cast<const uint8_t*>(tlv) + len);
        }

        if ((le16toh(recordSetSrc->record_set_id) != rsi && rsi != 0))
        {
            std::copy(table.begin() + pos, table.begin() + pos + recordLen,
                      std::back_inserter(updatedFruTbl));
        }
        else
        {
            // Deleted record
            numRecs--;
        }

        pos += recordLen;
    }
    // Replace the old table with the updated one
    table = std::move(updatedFruTbl);
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

void FruImpl::buildIndividualFRU(const std::string& fruInterface,
                                 const std::string& fruObjectPath)
{
    // An exception will be thrown by getRecordInfo, if the item
    // D-Bus interface name specified in FRU_Master.json does
    // not have corresponding config jsons
    pldm_entity parent = {};
    pldm_entity entity{};
    pldm_entity parentEntity{};
    static uint32_t bmc_record_handle = 0;
    uint32_t newRecordHdl{};

    if (fruInterface == "xyz.openbmc_project.Inventory.Item.PCIeDevice")
    {
        // Delete the existing adapter PDR before creating new one
        info("Removing old PDRs {PATH}", "PATH", fruObjectPath);
        removeIndividualFRU(fruObjectPath);
    }

    try
    {
        entity.entity_type = parser.getEntityType(fruInterface);
        auto parentObj = pldm::utils::findParent(fruObjectPath);
        do
        {
            auto iter = objToEntityNode.find(parentObj);
            if (iter != objToEntityNode.end())
            {
                parent = iter->second;
                break;
            }
            parentObj = pldm::utils::findParent(parentObj);
        } while (parentObj != "/");

        pldm_entity_node* parent_node = nullptr;
        pldm_find_entity_ref_in_tree(entityTree, parent, &parent_node);

        uint16_t last_container_id = next_container_id(entityTree);
        auto node = pldm_entity_association_tree_add_entity(
            entityTree, &entity, 0xFFFF, parent_node,
            PLDM_ENTITY_ASSOCIAION_PHYSICAL, false, true, last_container_id);

        pldm_entity node_entity = pldm_entity_extract(node);
        objToEntityNode[fruObjectPath] = node_entity;
        info(
            "Building Individual FRU [{FRU_OBJ_PATH}] with entityid [ {ENTITY_TYPE}, {ENTITY_NUM}, {ENTITY_ID} ] Parent :[ {P_ENTITY_TYP}, {P_ENTITY_NUM}, {P_ENTITY_ID} ]",
            "FRU_OBJ_PATH", fruObjectPath, "ENTITY_TYPE",
            static_cast<unsigned>(node_entity.entity_type), "ENTITY_NUM",
            static_cast<unsigned>(node_entity.entity_instance_num), "ENTITY_ID",
            static_cast<unsigned>(node_entity.entity_container_id),
            "P_ENTITY_TYP", static_cast<unsigned>(parent.entity_type),
            "P_ENTITY_NUM", static_cast<unsigned>(parent.entity_instance_num),
            "P_ENTITY_ID", static_cast<unsigned>(parent.entity_container_id));
        auto recordInfos = parser.getRecordInfo(fruInterface);

        memcpy(reinterpret_cast<void*>(&parentEntity),
               reinterpret_cast<void*>(parent_node), sizeof(pldm_entity));
        pldm_entity_node* bmcTreeParentNode = nullptr;
        pldm_find_entity_ref_in_tree(bmcEntityTree, parentEntity,
                                     &bmcTreeParentNode);

        pldm_entity_association_tree_add_entity(
            bmcEntityTree, &entity, 0xFFFF, bmcTreeParentNode,
            PLDM_ENTITY_ASSOCIAION_PHYSICAL, false, true, last_container_id);

        objects = DBusHandler::getManagedObj(
            "xyz.openbmc_project.Inventory.Manager", inventoryPath);
        for (const auto& object : objects)
        {
            if (object.first.str == fruObjectPath)
            {
                const auto& interfaces = object.second;
                newRecordHdl = populateRecords(interfaces, recordInfos, entity,
                                               fruObjectPath, true);
                associatedEntityMap.emplace(fruObjectPath, entity);
                break;
            }
        }
    }
    catch (const std::exception& e)
    {
        error(
            "Config JSONs missing for the item in concurrent add path interface type, interface = {FRU_INTF} ERROR={ERROR}",
            "FRU_INTF", fruInterface, "ERROR", e);
    }
    if (oemUtilsHandler)
    {
        auto lastLocalRecord = pldm_pdr_find_last_in_range(
            pdrRepo, BMC_PDR_START_RANGE, BMC_PDR_END_RANGE);
        bmc_record_handle =
            pldm_pdr_get_record_handle(pdrRepo, lastLocalRecord);
    }

    uint8_t bmcEventDataOps;
    uint32_t updatedRecordHdlBmc = 0;
    bool found = false;
    pldm_entity_association_find_parent_entity(pdrRepo, &parent, false,
                                               &updatedRecordHdlBmc, &found);
    if (found)
    {
        pldm_entity_association_pdr_add_contained_entity_to_remote_pdr(
            pdrRepo, &entity, updatedRecordHdlBmc);
        bmcEventDataOps = PLDM_RECORDS_MODIFIED;
    }
    else
    {
        pldm_entity_association_pdr_create_new(
            pdrRepo, bmc_record_handle, &parent, &entity, &updatedRecordHdlBmc);
        bmcEventDataOps = PLDM_RECORDS_ADDED;
    }

    if (oemUtilsHandler)
    {
        auto lastBMCRecord = pldm_pdr_find_last_in_range(
            pdrRepo, BMC_PDR_START_RANGE, BMC_PDR_END_RANGE);
        bmc_record_handle = pldm_pdr_get_record_handle(pdrRepo, lastBMCRecord);
    }

    uint8_t hostEventDataOps;
    uint32_t updatedRecordHdlHost = 0;
    found = false;
    pldm_entity_association_find_parent_entity(pdrRepo, &parent, true,
                                               &updatedRecordHdlHost, &found);
    if (found)
    {
        pldm_entity_association_pdr_add_contained_entity_to_remote_pdr(
            pdrRepo, &entity, updatedRecordHdlHost);
        hostEventDataOps = PLDM_RECORDS_MODIFIED;
    }

    // create the relevant state effecter and sensor PDRs for the new fru record
    std::vector<uint32_t> recordHdlList;
    reGenerateStatePDR(fruObjectPath, recordHdlList);

    std::vector<ChangeEntry> handlesTobeAdded;
    std::vector<ChangeEntry> handlesTobeModified;

    handlesTobeAdded.push_back(newRecordHdl);

    for (auto& ids : recordHdlList)
    {
        handlesTobeAdded.push_back(ids);
    }
    if (updatedRecordHdlBmc != 0)
    {
        (bmcEventDataOps == PLDM_RECORDS_MODIFIED)
            ? handlesTobeModified.push_back(updatedRecordHdlBmc)
            : handlesTobeAdded.push_back(updatedRecordHdlBmc);
    }
    if (updatedRecordHdlHost != 0)
    {
        (hostEventDataOps == PLDM_RECORDS_MODIFIED)
            ? handlesTobeModified.push_back(updatedRecordHdlHost)
            : handlesTobeAdded.push_back(updatedRecordHdlHost);
    }
    if (!handlesTobeAdded.empty())
    {
        sendPDRRepositoryChgEventbyPDRHandles(
            std::move(handlesTobeAdded),
            (std::vector<uint8_t>(1, PLDM_RECORDS_ADDED)));
    }
    if (!handlesTobeModified.empty())
    {
        sendPDRRepositoryChgEventbyPDRHandles(
            std::move(handlesTobeModified),
            (std::vector<uint8_t>(1, PLDM_RECORDS_MODIFIED)));
    }
}

void FruImpl::reGenerateStatePDR(const std::string& fruObjectPath,
                                 std::vector<uint32_t>& recordHdlList)
{
    pldm::responder::pdr_utils::Type pdrType{};
    static const Json empty{};
    for (const auto& directory : statePDRJsonsDir)
    {
        for (const auto& dirEntry : fs::directory_iterator(directory))
        {
            try
            {
                if (fs::is_regular_file(dirEntry.path().string()))
                {
                    auto json = pldm::responder::pdr_utils::readJson(
                        dirEntry.path().string());
                    if (!json.empty())
                    {
                        pldm::responder::pdr_utils::DbusObjMaps tmpMap{};
                        auto effecterPDRs = json.value("effecterPDRs", empty);
                        for (const auto& effecter : effecterPDRs)
                        {
                            pdrType = effecter.value("pdrType", 0);
                            if (pdrType == PLDM_STATE_EFFECTER_PDR)
                            {
                                auto stateEffecterList = setStatePDRParams(
                                    {directory}, 0, 0, tmpMap, tmpMap, true,
                                    effecter, fruObjectPath, pdrType);
                                std::move(stateEffecterList.begin(),
                                          stateEffecterList.end(),
                                          std::back_inserter(recordHdlList));
                            }
                        }
                        auto sensorPDRs = json.value("sensorPDRs", empty);
                        for (const auto& sensor : sensorPDRs)
                        {
                            pdrType = sensor.value("pdrType", 0);
                            if (pdrType == PLDM_STATE_SENSOR_PDR)
                            {
                                auto stateSensorList = setStatePDRParams(
                                    {directory}, 0, 0, tmpMap, tmpMap, true,
                                    sensor, fruObjectPath, pdrType);
                                std::move(stateSensorList.begin(),
                                          stateSensorList.end(),
                                          std::back_inserter(recordHdlList));
                            }
                        }
                    }
                }
            }
            catch (const InternalFailure& e)
            {
                error(
                    "PDR config directory for path PATH= {DIR_PATH} does not exist or is empty for TYPE= {PDR_TYP}, ERROR={ERROR}",
                    "DIR_PATH", dirEntry.path().string(), "PDR_TYP",
                    (unsigned)pdrType, "ERROR", e);
                // log an error here
            }
            catch (const Json::exception& e)
            {
                error(
                    "Failed parsing PDR JSON file for TYPE= {PDR_TYP} ERROR={ERROR}",
                    "PDR_TYP", (unsigned)pdrType, "ERROR", e);
                // log error
            }
            catch (const std::exception& e)
            {
                error(
                    "Failed parsing PDR JSON file for TYPE= {PDR_TYP} ERROR={ERROR}",
                    "PDR_TYP", (unsigned)pdrType, "ERROR", e);
                // log appropriate error
            }
        }
    }
}

std::vector<uint32_t> FruImpl::setStatePDRParams(
    const std::vector<fs::path> pdrJsonsDir, uint16_t nextSensorId,
    uint16_t nextEffecterId,
    pldm::responder::pdr_utils::DbusObjMaps& sensorDbusObjMaps,
    pldm::responder::pdr_utils::DbusObjMaps& effecterDbusObjMaps, bool hotPlug,
    const Json& json, const std::string& fruObjectPath,
    pldm::responder::pdr_utils::Type pdrType)
{
    using namespace pldm::responder::pdr_utils;
    static DbusObjMaps& sensorDbusObjMapsRef = sensorDbusObjMaps;
    static DbusObjMaps& effecterDbusObjMapsRef = effecterDbusObjMaps;
    std::vector<uint32_t> idList;
    static const Json empty{};
    if (!hotPlug)
    {
        startStateSensorId = nextSensorId;
        startStateEffecterId = nextEffecterId;
        statePDRJsonsDir = pdrJsonsDir;
        return idList;
    }

    if (pdrType == PLDM_STATE_EFFECTER_PDR)
    {
        static const std::vector<Json> emptyList{};
        auto entries = json.value("entries", emptyList);
        for (const auto& e : entries)
        {
            size_t pdrSize = 0;
            auto effecters = e.value("effecters", emptyList);
            for (const auto& effecter : effecters)
            {
                auto set = effecter.value("set", empty);
                auto statesSize = set.value("size", 0);
                if (!statesSize)
                {
                    error(
                        "Malformed PDR JSON return pdrEntry;- no state set info, TYPE={INFO_TYP}",
                        "INFO_TYP",
                        static_cast<unsigned>(PLDM_STATE_EFFECTER_PDR));
                    throw InternalFailure();
                }
                pdrSize += sizeof(state_effecter_possible_states) -
                           sizeof(bitfield8_t) +
                           (sizeof(bitfield8_t) * statesSize);
            }
            pdrSize += sizeof(pldm_state_effecter_pdr) - sizeof(uint8_t);

            std::vector<uint8_t> entry{};
            entry.resize(pdrSize);

            pldm_state_effecter_pdr* pdr =
                reinterpret_cast<pldm_state_effecter_pdr*>(entry.data());
            if (!pdr)
            {
                error("Failed to get state effecter PDR.");
                continue;
            }
            pdr->hdr.record_handle = 0;
            pdr->hdr.version = 1;
            pdr->hdr.type = PLDM_STATE_EFFECTER_PDR;
            pdr->hdr.record_change_num = 0;
            pdr->hdr.length = pdrSize - sizeof(pldm_pdr_hdr);

            pdr->terminus_handle = TERMINUS_HANDLE;

            bool singleEffecter = false;
            try
            {
                std::string entity_path = e.value("entity_path", "");
                if (fruObjectPath.size())
                {
                    if (fruObjectPath != entity_path)
                    {
                        continue;
                    }
                    singleEffecter = true;
                }
                pdr->effecter_id = startStateEffecterId++;
                if (entity_path != "" &&
                    associatedEntityMap.find(entity_path) !=
                        associatedEntityMap.end())
                {
                    pdr->entity_type =
                        associatedEntityMap.at(entity_path).entity_type;
                    pdr->entity_instance =
                        associatedEntityMap.at(entity_path).entity_instance_num;
                    pdr->container_id =
                        associatedEntityMap.at(entity_path).entity_container_id;
                }
                else
                {
                    pdr->entity_type = e.value("type", 0);
                    pdr->entity_instance = e.value("instance", 0);
                    pdr->container_id = e.value("container", 0);
                }
            }
            catch (const std::exception&)
            {
                pdr->entity_type = e.value("type", 0);
                pdr->entity_instance = e.value("instance", 0);
                pdr->container_id = e.value("container", 0);
            }
            pdr->effecter_semantic_id = 0;
            pdr->effecter_init = PLDM_NO_INIT;
            pdr->has_description_pdr = false;
            pdr->composite_effecter_count = effecters.size();

            pldm::responder::pdr_utils::DbusMappings dbusMappings{};
            pldm::responder::pdr_utils::DbusValMaps dbusValMaps{};
            uint8_t* start = entry.data() + sizeof(pldm_state_effecter_pdr) -
                             sizeof(uint8_t);
            for (const auto& effecter : effecters)
            {
                auto set = effecter.value("set", empty);
                state_effecter_possible_states* possibleStates =
                    reinterpret_cast<state_effecter_possible_states*>(start);
                possibleStates->state_set_id = set.value("id", 0);
                possibleStates->possible_states_size = set.value("size", 0);

                start += sizeof(possibleStates->state_set_id) +
                         sizeof(possibleStates->possible_states_size);
                static const std::vector<uint8_t> emptyStates{};
                pldm::responder::pdr_utils::PossibleValues stateValues;
                auto states = set.value("states", emptyStates);
                for (const auto& state : states)
                {
                    auto index = state / 8;
                    auto bit = state - (index * 8);
                    bitfield8_t* bf =
                        reinterpret_cast<bitfield8_t*>(start + index);
                    bf->byte |= 1 << bit;
                    stateValues.emplace_back(state);
                }
                start += possibleStates->possible_states_size;

                auto dbusEntry = effecter.value("dbus", empty);
                auto objectPath = dbusEntry.value("path", "");
                auto interface = dbusEntry.value("interface", "");
                auto propertyName = dbusEntry.value("property_name", "");
                auto propertyType = dbusEntry.value("property_type", "");

                pldm::responder::pdr_utils::StatestoDbusVal dbusIdToValMap{};
                pldm::utils::DBusMapping dbusMapping{};
                try
                {
                    auto service = pldm::utils::DBusHandler().getService(
                        objectPath.c_str(), interface.c_str());
                    dbusMapping = pldm::utils::DBusMapping{
                        objectPath, interface, propertyName, propertyType};
                    dbusIdToValMap =
                        pldm::responder::pdr_utils::populateMapping(
                            propertyType, dbusEntry["property_values"],
                            stateValues);
                }
                catch (const std::exception& e)
                {
                    error(
                        "D-Bus object path does not exist, effecter ID: {EFFECTER_ID} ERROR={ERROR}",
                        "EFFECTER_ID", static_cast<uint16_t>(pdr->effecter_id),
                        "ERROR", e);
                }
                dbusMappings.emplace_back(std::move(dbusMapping));
                dbusValMaps.emplace_back(std::move(dbusIdToValMap));
            }
            uint32_t effecterId = pdr->effecter_id;
            effecterDbusObjMapsRef.emplace(
                effecterId, std::make_tuple(std::move(dbusMappings),
                                            std::move(dbusValMaps)));
            pldm::responder::pdr_utils::PdrEntry pdrEntry{};
            pdrEntry.data = entry.data();
            pdrEntry.size = pdrSize;
            if (singleEffecter)
            {
                auto newRecordHdl = addHotPlugRecord(pdrEntry);
                idList.push_back(newRecordHdl);
            }
        }
    }
    else if (pdrType == PLDM_STATE_SENSOR_PDR)
    {
        static const std::vector<Json> emptyList{};
        auto entries = json.value("entries", emptyList);
        for (const auto& e : entries)
        {
            size_t pdrSize = 0;
            auto sensors = e.value("sensors", emptyList);
            for (const auto& sensor : sensors)
            {
                auto set = sensor.value("set", empty);
                auto statesSize = set.value("size", 0);
                if (!statesSize)
                {
                    error(
                        "Malformed PDR JSON return pdrEntry;- no state set info, TYPE={INFO_TYP}",
                        "INFO_TYP",
                        static_cast<unsigned>(PLDM_STATE_SENSOR_PDR));
                    throw InternalFailure();
                }
                pdrSize += sizeof(state_sensor_possible_states) -
                           sizeof(bitfield8_t) +
                           (sizeof(bitfield8_t) * statesSize);
            }
            pdrSize += sizeof(pldm_state_sensor_pdr) - sizeof(uint8_t);

            std::vector<uint8_t> entry{};
            entry.resize(pdrSize);

            pldm_state_sensor_pdr* pdr =
                reinterpret_cast<pldm_state_sensor_pdr*>(entry.data());
            if (!pdr)
            {
                error("Failed to get state sensor PDR.");
                continue;
            }
            pdr->hdr.record_handle = 0;
            pdr->hdr.version = 1;
            pdr->hdr.type = PLDM_STATE_SENSOR_PDR;
            pdr->hdr.record_change_num = 0;
            pdr->hdr.length = pdrSize - sizeof(pldm_pdr_hdr);

            HTOLE32(pdr->hdr.record_handle);
            HTOLE16(pdr->hdr.record_change_num);
            HTOLE16(pdr->hdr.length);

            pdr->terminus_handle = TERMINUS_HANDLE;
            bool singleSensor = false;

            try
            {
                std::string entity_path = e.value("entity_path", "");
                if (fruObjectPath.size())
                {
                    if (fruObjectPath != entity_path)
                    {
                        continue;
                    }
                    singleSensor = true;
                }
                pdr->sensor_id = startStateSensorId++;
                if (entity_path != "" &&
                    associatedEntityMap.find(entity_path) !=
                        associatedEntityMap.end())
                {
                    pdr->entity_type =
                        associatedEntityMap.at(entity_path).entity_type;
                    pdr->entity_instance =
                        associatedEntityMap.at(entity_path).entity_instance_num;
                    pdr->container_id =
                        associatedEntityMap.at(entity_path).entity_container_id;
                }
                else
                {
                    pdr->entity_type = e.value("type", 0);
                    pdr->entity_instance = e.value("instance", 0);
                    pdr->container_id = e.value("container", 0);
                }
            }
            catch (const std::exception&)
            {
                pdr->entity_type = e.value("type", 0);
                pdr->entity_instance = e.value("instance", 0);
                pdr->container_id = e.value("container", 0);
            }
            pdr->sensor_init = PLDM_NO_INIT;
            pdr->sensor_auxiliary_names_pdr = false;
            if (sensors.size() > 8)
            {
                throw std::runtime_error("sensor size must be less than 8");
            }
            pdr->composite_sensor_count = sensors.size();

            HTOLE16(pdr->terminus_handle);
            HTOLE16(pdr->sensor_id);
            HTOLE16(pdr->entity_type);
            HTOLE16(pdr->entity_instance);
            HTOLE16(pdr->container_id);

            pldm::responder::pdr_utils::DbusMappings dbusMappings{};
            pldm::responder::pdr_utils::DbusValMaps dbusValMaps{};
            uint8_t* start =
                entry.data() + sizeof(pldm_state_sensor_pdr) - sizeof(uint8_t);
            for (const auto& sensor : sensors)
            {
                auto set = sensor.value("set", empty);
                state_sensor_possible_states* possibleStates =
                    reinterpret_cast<state_sensor_possible_states*>(start);
                possibleStates->state_set_id = set.value("id", 0);
                HTOLE16(possibleStates->state_set_id);
                possibleStates->possible_states_size = set.value("size", 0);

                start += sizeof(possibleStates->state_set_id) +
                         sizeof(possibleStates->possible_states_size);
                static const std::vector<uint8_t> emptyStates{};
                pldm::responder::pdr_utils::PossibleValues stateValues;
                auto states = set.value("states", emptyStates);
                for (const auto& state : states)
                {
                    auto index = state / 8;
                    auto bit = state - (index * 8);
                    bitfield8_t* bf =
                        reinterpret_cast<bitfield8_t*>(start + index);
                    bf->byte |= 1 << bit;
                    stateValues.emplace_back(state);
                }
                start += possibleStates->possible_states_size;
                auto dbusEntry = sensor.value("dbus", empty);
                auto objectPath = dbusEntry.value("path", "");
                auto interface = dbusEntry.value("interface", "");
                auto propertyName = dbusEntry.value("property_name", "");
                auto propertyType = dbusEntry.value("property_type", "");

                pldm::responder::pdr_utils::StatestoDbusVal dbusIdToValMap{};
                pldm::utils::DBusMapping dbusMapping{};
                try
                {
                    auto service = pldm::utils::DBusHandler().getService(
                        objectPath.c_str(), interface.c_str());
                    dbusMapping = pldm::utils::DBusMapping{
                        objectPath, interface, propertyName, propertyType};
                    dbusIdToValMap =
                        pldm::responder::pdr_utils::populateMapping(
                            propertyType, dbusEntry["property_values"],
                            stateValues);
                }
                catch (const std::exception& e)
                {
                    error(
                        "D-Bus object path does not exist, sensor ID: {SENSOR_ID} ERROR={ERROR}",
                        "SENSOR_ID", static_cast<uint16_t>(pdr->sensor_id),
                        "ERROR", e);
                }
                dbusMappings.emplace_back(std::move(dbusMapping));
                dbusValMaps.emplace_back(std::move(dbusIdToValMap));
            }
            uint32_t sensorId = pdr->sensor_id;
            sensorDbusObjMapsRef.emplace(
                sensorId, std::make_tuple(std::move(dbusMappings),
                                          std::move(dbusValMaps)));

            // creating a match for the newly added sensor
            dbusToPLDMEventHandler->sendStateSensorEvent(sensorId,
                                                         sensorDbusObjMapsRef);

            pldm::responder::pdr_utils::PdrEntry pdrEntry{};
            pdrEntry.data = entry.data();
            pdrEntry.size = pdrSize;
            if (singleSensor)
            {
                auto newRecordHdl = addHotPlugRecord(pdrEntry);
                idList.push_back(newRecordHdl);
            }
        }
    }
    return idList;
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
