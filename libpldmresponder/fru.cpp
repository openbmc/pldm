#include "fru.hpp"

#include <libpldm/entity.h>
#include <libpldm/utils.h>
#include <libpldm/pdr.h>
#include <systemd/sd-journal.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>

#include <iostream>
#include <optional>
#include <set>
#include <stack>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace responder
{

constexpr auto root = "/xyz/openbmc_project/inventory/";
constexpr uint32_t BMC_PDR_START_RANGE = 0x00000000;
constexpr uint32_t BMC_PDR_END_RANGE = 0x00FFFFFF;

std::optional<pldm_entity>
    FruImpl::getEntityByObjectPath(const dbus::InterfaceMap& intfMaps)
{
    for (const auto& intfMap : intfMaps)
    {
        try
        {
            pldm_entity entity{};
            entity.entity_type = parser.getEntityType(intfMap.first);
            return entity;
        }
        catch (const std::exception& e)
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
    // Update pldm entity to assocition tree
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

                for (auto& it : objToEntityNode)
                {
                    pldm_entity node = pldm_entity_extract(it.second);
                    if (node.entity_type == entity.entity_type)
                    {
                        entity.entity_instance_num = node.entity_instance_num +
                                                     1;
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
    // Read the all the inventory D-Bus objects
    auto& bus = pldm::utils::DBusHandler::getBus();
    dbus::ObjectValueTree objects;

    try
    {
        dbusInfo = parser.inventoryLookup();
        auto method = bus.new_method_call(
            std::get<0>(dbusInfo).c_str(), std::get<1>(dbusInfo).c_str(),
            "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
        auto reply = bus.call(method, dbusTimeout);
        reply.read(objects);
    }
    catch (const std::exception& e)
    {
        error(
            "Look up of inventory objects failed and PLDM FRU table creation failed");
        return;
    }

    auto itemIntfsLookup = std::get<2>(dbusInfo);

    for (const auto& object : objects)
    {
        const auto& interfaces = object.second;
        bool isPresent = pldm::utils::checkForFruPresence(object.first.str);
        // Do not create fru record if fru is not present.
        // Pick up the next available fru.
        if (!isPresent)
        {
            continue;
        }
        for (const auto& interface : interfaces)
        {
            if (itemIntfsLookup.find(interface.first) != itemIntfsLookup.end())
            {
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
                        "Config JSONs missing for the item interface type, interface = {INTF}",
                        "INTF", interface.first);
                    break;
                }
            }
        }
    }

    int rc = pldm_entity_association_pdr_add_check(entityTree, pdrRepo, false,
                                                   TERMINUS_HANDLE);
    if (rc < 0)
    {
        // pldm_entity_assocation_pdr_add() assert()ed on failure
        error("Failed to add PLDM entity association PDR: {LIBPLDM_ERROR}",
              "LIBPLDM_ERROR", rc);
        throw std::runtime_error("Failed to add PLDM entity association PDR");
    }

    // save a copy of bmc's entity association tree
    pldm_entity_association_tree_copy_root(entityTree, bmcEntityTree);

    if (table.size())
    {
        padBytes = pldm::utils::getNumPadBytes(table.size());
        table.resize(table.size() + padBytes, 0);

        // Calculate the checksum
        checksum = crc32(table.data(), table.size());
    }
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
        auto method = bus.new_method_call(pldm::utils::mapperService,
                                          fwFunctionalObjPath,
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
        error("failed to make a d-bus call Asociation, ERROR= {ERR_EXCEP}",
              "ERR_EXCEP", e.what());
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
            catch (const std::out_of_range& e)
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
                int rc = pldm_pdr_add_fru_record_set_check(
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
                    "PDR config directory does not exist or empty, TYPE= {PDR_TYP} PATH= {DIR_PATH} ERROR={ERR_EXCEP}",
                    "PDR_TYP", (unsigned)pdrType, "DIR_PATH",
                    dirEntry.path().string(), "ERR_EXCEP", e.what());
                // log an error here
            }
            catch (const Json::exception& e)
            {
                error(
                    "Failed parsing PDR JSON file, TYPE= {PDR_TYP} ERROR={ERR_EXCEP}",
                    "PDR_TYP", (unsigned)pdrType, "ERR_EXCEP", e.what());
                // log error
            }
            catch (const std::exception& e)
            {
                error(
                    "Failed parsing PDR JSON file, TYPE= {PDR_TYP} ERROR={ERR_EXCEP}",
                    "PDR_TYP", (unsigned)pdrType, "ERR_EXCEP", e.what());
                // log appropriate error
            }
        }
    }
}

void FruImpl::getFRUTable(Response& response)
{
    auto hdrSize = response.size();

    response.resize(hdrSize + table.size() + sizeof(checksum), 0);
    std::copy(table.begin(), table.end(), response.begin() + hdrSize);

    // Copy the checksum to response data
    auto iter = response.begin() + hdrSize + table.size();
    std::copy_n(reinterpret_cast<const uint8_t*>(&checksum), sizeof(checksum),
                iter);
}

int FruImpl::getFRURecordByOption(std::vector<uint8_t>& fruData,
                                  uint16_t /* fruTableHandle */,
                                  uint16_t recordSetIdentifer,
                                  uint8_t recordType, uint8_t fieldType)
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

    int rc = get_fru_record_by_option_check(
        table.data(), table.size() - padBytes, fruData.data(), &recordTableSize,
        recordSetIdentifer, recordType, fieldType);

    if (rc != PLDM_SUCCESS || recordTableSize == 0)
    {
        return PLDM_FRU_DATA_STRUCTURE_TABLE_UNAVAILABLE;
    }

    auto pads = pldm::utils::getNumPadBytes(recordTableSize);
    crc32(fruData.data(), recordTableSize + pads);

    auto iter = fruData.begin() + recordTableSize + pads;
    std::copy_n(reinterpret_cast<const uint8_t*>(&checksum), sizeof(checksum),
                iter);
    fruData.resize(recordTableSize + pads + sizeof(sum));

    return PLDM_SUCCESS;
}

uint32_t
    FruImpl::addHotPlugRecord(pldm::responder::pdr_utils::PdrEntry pdrEntry)
{
    uint32_t lastHandle = 0;
#ifdef OEM_IBM
    auto lastLocalRecord = (pldm_pdr_record*)pldm_pdr_find_last_in_range(
        pdrRepo, BMC_PDR_START_RANGE, BMC_PDR_END_RANGE);
    lastHandle = lastLocalRecord->record_handle;
#endif
    pdrEntry.handle.recordHandle = lastHandle + 1;
    return pldm_pdr_add_hotplug_record(pdrRepo, pdrEntry.data, pdrEntry.size,
                                       pdrEntry.handle.recordHandle, false,
                                       lastHandle, TERMINUS_HANDLE);
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

            // pdr->terminus_handle = pdr::BmcPldmTerminusHandle;
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
                pdr->effecter_id =
                    startStateEffecterId++; // handler.getNextEffecterId();
                // auto& associatedEntityMap = handler.getAssociateEntityMap();
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
            catch (const std::exception& ex)
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
                    auto service =
                        // dBusIntf.getService(objectPath.c_str(),
                        // interface.c_str());
                        pldm::utils::DBusHandler().getService(
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
                        "D-Bus object path does not exist, effecter ID: {EFFECTER_ID}",
                        "EFFECTER_ID", static_cast<uint16_t>(pdr->effecter_id));
                }
                dbusMappings.emplace_back(std::move(dbusMapping));
                dbusValMaps.emplace_back(std::move(dbusIdToValMap));
            }
            /* handler.addDbusObjMaps(
                 pdr->effecter_id,
                 std::make_tuple(std::move(dbusMappings),
               std::move(dbusValMaps)));*/
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
                // nowa dd to the vector
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
                pdr->sensor_id =
                    startStateSensorId++; // handler.getNextSensorId();
                // auto& associatedEntityMap = handler.getAssociateEntityMap();
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
            catch (const std::exception& ex)
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
            uint8_t* start = entry.data() + sizeof(pldm_state_sensor_pdr) -
                             sizeof(uint8_t);
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
                    auto service =
                        // dBusIntf.getService(objectPath.c_str(),
                        // interface.c_str());
                        pldm::utils::DBusHandler().getService(
                            objectPath.c_str(), interface.c_str());
                    dbusMapping = pldm::utils::DBusMapping{
                        objectPath, interface, propertyName, propertyType};
                    dbusIdToValMap =
                        pldm::responder::pdr_utils::populateMapping(
                            propertyType, dbusEntry["property_values"],
                            stateValues);
                }
                catch (const std::exception&)
                {
                    error(
                        "D-Bus object path does not exist, sensor ID: {SENSOR_ID}",
                        "SENSOR_ID", static_cast<uint16_t>(pdr->sensor_id));
                }
                dbusMappings.emplace_back(std::move(dbusMapping));
                dbusValMaps.emplace_back(std::move(dbusIdToValMap));
            }
            /*handler.addDbusObjMaps(
                pdr->sensor_id,
                std::make_tuple(std::move(dbusMappings),
               std::move(dbusValMaps)),
                pldm::responder::pdr_utils::TypeId::PLDM_SENSOR_ID);*/
            uint32_t sensorId = pdr->sensor_id;
            sensorDbusObjMapsRef.emplace(
                sensorId, std::make_tuple(std::move(dbusMappings),
                                          std::move(dbusValMaps)));

            pldm::responder::pdr_utils::PdrEntry pdrEntry{};
            pdrEntry.data = entry.data();
            pdrEntry.size = pdrSize;
            if (singleSensor)
            {
                auto newRecordHdl = addHotPlugRecord(pdrEntry);
                idList.push_back(newRecordHdl);
            }
            std::cout << "\nexit FruImpl::setStatePDRParams" << std::endl;
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
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

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
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    auto rc = encode_get_fru_record_table_resp(request->hdr.instance_id,
                                               PLDM_SUCCESS, 0,
                                               PLDM_START_AND_END, responsePtr);
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

    auto respPayloadLength = PLDM_GET_FRU_RECORD_BY_OPTION_MIN_RESP_BYTES +
                             fruData.size();
    Response response(sizeof(pldm_msg_hdr) + respPayloadLength, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    rc = encode_get_fru_record_by_option_resp(
        request->hdr.instance_id, PLDM_SUCCESS, 0, PLDM_START_AND_END,
        fruData.data(), fruData.size(), responsePtr, respPayloadLength);

    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    return response;
}

void Handler::setStatePDRParams(
    const std::vector<fs::path> pdrJsonsDir, uint16_t nextSensorId,
    uint16_t nextEffecterId,
    pldm::responder::pdr_utils::DbusObjMaps& sensorDbusObjMaps,
    pldm::responder::pdr_utils::DbusObjMaps& effecterDbusObjMaps, bool hotPlug)
{
    impl.setStatePDRParams(pdrJsonsDir, nextSensorId, nextEffecterId,
                           sensorDbusObjMaps, effecterDbusObjMaps, hotPlug,
                           Json());
}

} // namespace fru

} // namespace responder

} // namespace pldm
