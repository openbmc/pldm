#include "fru.hpp"

#include "common/utils.hpp"

#include <libpldm/entity.h>
#include <libpldm/utils.h>
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
        std::cout << s << std::endl;
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
        auto reply = bus.call(
            method,
            std::chrono::duration_cast<microsec>(sec(DBUS_TIMEOUT)).count());
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
                    info(
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
        auto reply = bus.call(
            method,
            std::chrono::duration_cast<microsec>(sec(DBUS_TIMEOUT)).count());
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

} // namespace fru

} // namespace responder

} // namespace pldm
