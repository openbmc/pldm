#include "fru.hpp"

#include "libpldm/utils.h"

#include "common/utils.hpp"

#include <systemd/sd-journal.h>

#include <sdbusplus/bus.hpp>

#include <iostream>
#include <regex>
#include <set>

namespace pldm
{

namespace responder
{

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
        auto reply = bus.call(method);
        reply.read(objects);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Look up of inventory objects failed and PLDM FRU table "
                     "creation failed";
        return;
    }

    auto itemIntfsLookup = std::get<2>(dbusInfo);

    for (const auto& object : objects)
    {
        const auto& interfaces = object.second;

        for (const auto& interface : interfaces)
        {
            if (itemIntfsLookup.find(interface.first) != itemIntfsLookup.end())
            {
                // An exception will be thrown by getRecordInfo, if the item
                // D-Bus interface name specified in FRU_Master.json does
                // not have corresponding config jsons
                try
                {
                    pldm_entity entity{};
                    entity.entity_type = parser.getEntityType(interface.first);
                    pldm_entity_node* parent = nullptr;
                    auto parentObj = pldm::utils::findParent(object.first.str);
                    // To add a FRU to the entity association tree, we need to
                    // determine if the FRU has a parent (D-Bus object). For eg
                    // /system/backplane's parent is /system. /system has no
                    // parent. Some D-Bus pathnames might just be namespaces
                    // (not D-Bus objects), so we need to iterate upwards until
                    // a parent is found, or we reach the root ("/").
                    // Parents are always added first before children in the
                    // entity association tree. We're relying on the fact that
                    // the std::map containing object paths from the
                    // GetManagedObjects call will have a sorted pathname list.
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

                    auto node = pldm_entity_association_tree_add(
                        entityTree, &entity, parent,
                        PLDM_ENTITY_ASSOCIAION_PHYSICAL);
                    objToEntityNode[object.first.str] = node;

                    auto recordInfos = parser.getRecordInfo(interface.first);

                    for (auto const& [recType, encType, fieldInfos] :
                         recordInfos)
                    {
                        for (auto const& [intf, prop, propType, fieldTypeNum] :
                             fieldInfos)
                        {
                            std::cout << "buildFRUTable \n";
                            std::cout << "recType " << (uint16_t)recType
                             << "propType " << propType 
                           << "fieldTypeNum " << (uint16_t)fieldTypeNum 
                             << "prop " << prop << std::endl;
                        }
                    }
                    std::cout << "calling populateRecords for entity type " 
                              << entity.entity_type << "\n";
                    populateRecords(interfaces, recordInfos, entity);

                    associatedEntityMap.emplace(object.first, entity);
                    break;
                }
                catch (const std::exception& e)
                {
                    std::cout << "Config JSONs missing for the item "
                                 "interface type, interface = "
                              << interface.first << "\n";
                    break;
                }
            }
        }
    }

    pldm_entity_association_pdr_add(entityTree, pdrRepo, false);

    if (table.size())
    {
        padBytes = utils::getNumPadBytes(table.size());
        table.resize(table.size() + padBytes, 0);

        // Calculate the checksum
        checksum = crc32(table.data(), table.size());
    }
    isBuilt = true;
}
void FruImpl::populatefwVersion()
{
    std::cout << "enter populatefwVersion \n";
    static constexpr auto fwFunctionalObjPath =
        "/xyz/openbmc_project/software/functional";
    static constexpr auto fwInterface = "org.freedesktop.DBus.Properties";
    static constexpr auto fwService = "xyz.openbmc_project.ObjectMapper";
    auto& bus = pldm::utils::DBusHandler::getBus();
    try
    {
        auto method = bus.new_method_call(fwService, fwFunctionalObjPath,
                                          fwInterface, "Get");
        method.append("xyz.openbmc_project.Association", "endpoints");
        std::variant<std::vector<std::string>> paths;
        auto reply = bus.call(method);
        reply.read(paths);
        fwRunningVersion = std::get<std::vector<std::string>>(paths)[0];
        std::cout << "Running Version:" << fwRunningVersion << std::endl;
        constexpr auto versionIntf = "xyz.openbmc_project.Software.Version";
        auto version = pldm::utils::DBusHandler().getDbusPropertyVariant(fwRunningVersion.c_str(),"Version",versionIntf);
        const auto& currentBmcVersion = std::get<std::string>(version);
        std::cout << "Current version installed in bmc: " << currentBmcVersion << "\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "failed to make a d-bus call "
                     "Asociation, ERROR= "
                  << e.what() << "\n";
        return;
    }
    std::regex version_regex("([^/]+)$");
    std::sregex_iterator i = std::sregex_iterator(
        fwRunningVersion.begin(), fwRunningVersion.end(), version_regex);
    auto image = *i;
    fwImageId = image.str();
}

void FruImpl::populateRecords(
    const pldm::responder::dbus::InterfaceMap& interfaces,
    const fru_parser::FruRecordInfos& recordInfos, const pldm_entity& entity)
{
    std::cout << "entered populateRecords with entity type " 
              << entity.entity_type << "\n";
    // recordSetIdentifier for the FRU will be set when the first record gets
    // added for the FRU
    uint16_t recordSetIdentifier = 0;
    auto numRecsCount = numRecs;

    for (auto const& [recType, encType, fieldInfos] : recordInfos)
    {
        std::vector<uint8_t> tlvs;
        uint8_t numFRUFields = 0;
        std::cout << "entering inner for loop \n";
        for (auto const& [intf, prop, propType, fieldTypeNum] : fieldInfos)
        {
            std::cout << "FieldTypeNum: " << (uint16_t)fieldTypeNum
                    << " Property type:" << propType
            << " Property: " << prop << std::endl;

            try
            {
                std::cout << "entered try block with propType " << propType << "\n";
                std::cout << "intf is: " << intf.c_str() << "\n";
                pldm::responder::dbus::Value propValue;
                if(propType == "string" && entity.entity_type == 11521
                                    && prop == "Version")
                {
                    std::cout << "match match match \n";
                }
                else
                {
                    std::cout << "calling dubus interface for " << propType.c_str() << " \n";
                    propValue = interfaces.at(intf).at(prop);
                }
                //auto propValue = interfaces.at(intf).at(prop);
                std::cout << "before starting comparison \n";
                if (propType == "bytearray")
                {
                    std::cout << "matched bytearray \n";
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
                    std::cout << "matched string \n";
                    if (entity.entity_type == 11521 && prop == "Version")
                    {
                        std::cout << " populatefwVersion method is to be called" 
                                << " for entity type " << entity.entity_type << std::endl;
                        populatefwVersion();
                        std::cout << " done:" << std::endl;
                        auto str = fetchnewImageId();
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
                    else
                    {
                        std::cout << "else part not calling populatefwVersion" << std::endl;
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
                pldm_pdr_add_fru_record_set(
                    pdrRepo, 0, recordSetIdentifier, entity.entity_type,
                    entity.entity_instance_num, entity.entity_container_id);
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
    // FRU table is built lazily, build if not done.
    buildFRUTable();

    /* 7 is sizeof(checksum,4) + padBytesMax(3)
     * We can not know size of the record table got by options in advance, but
     * it must be less than the source table. So it's safe to use sizeof the
     * source table + 7 as the buffer length
     */
    size_t recordTableSize = table.size() - padBytes + 7;
    fruData.resize(recordTableSize, 0);

    get_fru_record_by_option(table.data(), table.size() - padBytes,
                             fruData.data(), &recordTableSize,
                             recordSetIdentifer, recordType, fieldType);

    if (recordTableSize == 0)
    {
        return PLDM_FRU_DATA_STRUCTURE_TABLE_UNAVAILABLE;
    }

    auto pads = utils::getNumPadBytes(recordTableSize);
    auto sum = crc32(fruData.data(), recordTableSize + pads);

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
