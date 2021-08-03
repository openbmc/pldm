#include "fru.hpp"

#include "libpldm/entity.h"
#include "libpldm/utils.h"

#include "common/utils.hpp"

#include <systemd/sd-journal.h>

#include <sdbusplus/bus.hpp>

#include <iostream>
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
  //  dbus::ObjectValueTree objects; // now declared as a member in FruImpl

    try
    {
        dbusInfo = parser.inventoryLookup();
        std::cout << "service: " << std::get<0>(dbusInfo).c_str()
                  << " object: " << std::get<1>(dbusInfo).c_str() << "\n";
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
        std::cout << "\nobject.first " << object.first.str << "\n";
        auto isPresent = checkFruPresence(object.first.str.c_str());
        //may be a #ifder oem-ibm check here so that we build the pcie slot
        //and adpters and nvme slot fru records even if they are not present
        //because as per our agreemnet bmc is supposed to create empty
        //fru records for all indistry std adapters and
        //probably create fru records only for present ibm adapters
        //how to distinguish btn the two? can the PrettyName have
        //some ibm specific string? READ the CCIN or Decorator.Model property
        //also what do we do about nvme slots? check presence before 
        //building fru records?
        if(!isPresent)
        {
            std::cout << "\n fru " << object.first.str << " is not present,"
                      << "  will not build fru record now \n";
            continue;
        }
        else //REMOVE THIS LATER
        {
            std::cout << "building fru record for " << object.first.str<< "\n";
        }

        for (const auto& interface : interfaces)
        {
       //     std::cout << "interface.first " << interface.first << "\n";
            if (itemIntfsLookup.find(interface.first) != itemIntfsLookup.end())
            {
                std::cout << "\nbuilding fru record for interface " 
                          << interface.first << " and object " << object.first.str << "\n";;
                // An exception will be thrown by getRecordInfo, if the item
                // D-Bus interface name specified in FRU_Master.json does
                // not have corresponding config jsons
                try
                {
                    pldm_entity entity{};
                    entity.entity_type = parser.getEntityType(interface.first);
                    pldm_entity_node* parent = nullptr;
                    auto parentObj = pldm::utils::findParent(object.first.str);
                    std::cout << "found parent as " << parentObj <<
                               " for object " << object.first.str << "\n";
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

                    std::cout << "\ncalling pldm_entity_association_tree_add \n";
                    auto node = pldm_entity_association_tree_add(
                        entityTree, &entity, 0xFFFF, parent,
                        PLDM_ENTITY_ASSOCIAION_PHYSICAL);
                    objToEntityNode[object.first.str] = node;

                    std::cout << "\ncalling getRecordInfo on the interface"
                              << interface.first << "\n"; 
                    auto recordInfos = parser.getRecordInfo(interface.first);
                    std::cout << "\ncalling populateRecords \n";
                    populateRecords(interfaces, recordInfos, entity,object.first);

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
    // save a copy of bmc's entity association tree
    pldm_entity_association_tree_copy_root(entityTree, bmcEntityTree);

    if (table.size())
    {
        padBytes = utils::getNumPadBytes(table.size());
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
        auto method =
            bus.new_method_call(pldm::utils::mapperService, fwFunctionalObjPath,
                                pldm::utils::dbusProperties, "Get");
        method.append("xyz.openbmc_project.Association", "endpoints");
        std::variant<std::vector<std::string>> paths;
        auto reply = bus.call(method);
        reply.read(paths);
        auto fwRunningVersion = std::get<std::vector<std::string>>(paths)[0];
        constexpr auto versionIntf = "xyz.openbmc_project.Software.Version";
        auto version = pldm::utils::DBusHandler().getDbusPropertyVariant(
            fwRunningVersion.c_str(), "Version", versionIntf);
        currentBmcVersion = std::get<std::string>(version);
    }
    catch (const std::exception& e)
    {
        std::cerr << "failed to make a d-bus call "
                     "Asociation, ERROR= "
                  << e.what() << "\n";
        return {};
    }
    return currentBmcVersion;
}
void FruImpl::populateRecords(
    const pldm::responder::dbus::InterfaceMap& interfaces,
    const fru_parser::FruRecordInfos& recordInfos, const pldm_entity& entity,
    const dbus::ObjectPath& objectPath)
{
    std::cout << "\nenter populateRecords for object " << objectPath << " \n";
    std::cout << "\n numRecs is " << (uint32_t)numRecs << "\n";
    // recordSetIdentifier for the FRU will be set when the first record gets
    // added for the FRU
    uint16_t recordSetIdentifier = 0;
    auto numRecsCount = numRecs;
    static uint32_t bmc_record_handle = 0;

    for (auto const& [recType, encType, fieldInfos] : recordInfos)
    {
        std::cout << "populateRecords outer for loop \n";
        std::vector<uint8_t> tlvs;
        uint8_t numFRUFields = 0;
        for (auto const& [intf, prop, propType, fieldTypeNum] : fieldInfos)
        {
            std::cout << "\nfor loop intf " << intf << " prop " << prop << "\n";

            try
            {
                pldm::responder::dbus::Value propValue;
                if (entity.entity_type == PLDM_ENTITY_SYSTEM_LOGICAL &&
                    prop == "Version")
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
                std::cout << "\n numRecs and numRecsCount are same " << numRecsCount
                          << " calling pldm_pdr_add_fru_record_set \n";
                recordSetIdentifier = nextRSI();
                bmc_record_handle = nextRecordHandle();
                pldm_pdr_add_fru_record_set(
                    pdrRepo, 0, recordSetIdentifier, entity.entity_type,
                    entity.entity_instance_num, entity.entity_container_id,
                    bmc_record_handle);
                std::cout << "\nafter pldm_pdr_add_fru_record_set \n";
                objectPathToRSIMap[objectPath] = recordSetIdentifier;
                std::cout << "\n added to objectPathToRSIMap \n";
            }
            auto curSize = table.size();
            table.resize(curSize + recHeaderSize + tlvs.size());
            encode_fru_record(table.data(), table.size(), &curSize,
                              recordSetIdentifier, recType, numFRUFields,
                              encType, tlvs.data(), tlvs.size());
            numRecs++;
            std::cout << "\n numRecs increased to " << (uint32_t)numRecs << "\n";
        }
    }
}

void FruImpl::removeIndividualFRU(const std::string& fruObjPath)
{
    std::cout << "\nremoving fru record and entity association for " 
              << fruObjPath << "\n";
    //now delete fru record set PDR
    //and delete entity assoc PDR both local and remote
    //update both the bmc and combined entity assoc tree
    //recalculate the fru table checksum and table.resize
    
    uint16_t rsi = objectPathToRSIMap[fruObjPath];
    std::cout << "\n fru RSI to be deleted " << rsi << "\n";          
    pldm_entity removeEntity;
    uint16_t terminusHdl{};
    uint16_t entityType{};
    uint16_t entityInsNum{};
    uint16_t containerId{};
    pldm_pdr_fru_record_set_find_by_rsi(pdrRepo, rsi, &terminusHdl,
                       &entityType, &entityInsNum, 
                       &containerId, false);
    std::cout << "\nfru rsi found to be " << rsi << "\n";
    removeEntity.entity_type = entityType;
    removeEntity.entity_instance_num = entityInsNum;
    removeEntity.entity_container_id = containerId;
    std::cout << "\n removeEntity.entity_type " << removeEntity.entity_type << "\n";
    std::cout << "\n removeEntity.entity_instance_num " << removeEntity.entity_instance_num << "\n";
    std::cout << "\n removeEntity.entity_container_id " << removeEntity.entity_container_id << "\n";

    std::cout << "\ncalling pldm_entity_association_pdr_remove_contained_entity for local \n";
    pldm_entity_association_pdr_remove_contained_entity(pdrRepo,removeEntity,false);
    std::cout << "\nafter  pldm_entity_association_pdr_remove_contained_entity for local \n";

    pldm_entity_association_pdr_remove_contained_entity(pdrRepo,removeEntity,true);
    std::cout << "\nafter  pldm_entity_association_pdr_remove_contained_entity for remote \n";
    
    std::cout << "calling pldm_pdr_remove_fru_record_set_by_rsi for local \n";
    pldm_pdr_remove_fru_record_set_by_rsi(pdrRepo, rsi,false);

    std::cout << "\ncalling pldm_entity_association_tree_delete_node for combined tree \n";
    pldm_entity_association_tree_delete_node(entityTree,removeEntity);
    std::cout << "\ncalling pldm_entity_association_tree_delete_node for bmc tree \n";
    pldm_entity_association_tree_delete_node(bmcEntityTree,removeEntity);
    std::cout << "\nafter pldm_entity_association_tree_delete_node \n";
    objectPathToRSIMap.erase(fruObjPath);
    std::cout << "\nafter erasing " << fruObjPath << " from objectPathToRSIMap \n";

    if (table.size()) ///need to remove the entry from table before doing this
        //may be a separate commit to handle that. as of now it does not
        //create issue because the pdr repo is updated. the fru record table is
        //not. which means the pldmtool fru commands will still show the old
        //record. that may be fine since Host is not asking at this moment
        //they are getting the updated pdr (both fru and entity assoc pdr)
        //but eventually we need it because an add happened after a remove will
        //cause two fru records for the same fan
    {
        padBytes = utils::getNumPadBytes(table.size());
        table.resize(table.size() + padBytes, 0);
        // Calculate the checksum
        checksum = crc32(table.data(), table.size());
    }
    std::cout << "\n exit removeIndividualFRU \n";
}

void FruImpl::buildIndividualFRU(/*const dbus::Interfaces& itemIntfsLookup,*/ 
                   const std::string& fruInterface,const std::string& fruObjectPath) 
{
    std::cout << "\nenter buildIndividualFRU with object " 
              << "fruObjectPath and" << " interface " << fruInterface << " \n";
    /*if(itemIntfsLookup.find(fruInterface) == itemIntfsLookup.end())
    {
        std::cout << "\ndid not find the interface, will not build the fru" <<
            " record for object " << fruObjectPath  << " interface "
            << fruInterface << "\n";
        return;
    }*/
    // An exception will be thrown by getRecordInfo, if the item
    // D-Bus interface name specified in FRU_Master.json does
    // not have corresponding config jsons
    pldm_entity_node* parent = nullptr;
    pldm_entity entity{};
    try
    {
        entity.entity_type = parser.getEntityType(fruInterface);
        auto parentObj = pldm::utils::findParent(fruObjectPath);
        std::cout << "found parent as " << parentObj <<
                  " for object " << fruObjectPath << "\n";
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

        std::cout << "\ncalling pldm_entity_association_tree_add on the combined tree \n";
        auto node = pldm_entity_association_tree_add(
                    entityTree, &entity, 0xFFFF, parent,
                    PLDM_ENTITY_ASSOCIAION_PHYSICAL);
        objToEntityNode[fruObjectPath] = node;
        std::cout << "\ncalling getRecordInfo on the interface " << fruInterface << "\n";
        auto recordInfos = parser.getRecordInfo(fruInterface);

        std::cout << "\ncalling pldm_entity_association_tree_add on bmc tree \n";
        pldm_entity_association_tree_add(
                       bmcEntityTree, &entity, 0xFFFF, parent,
                       PLDM_ENTITY_ASSOCIAION_PHYSICAL);
        std::cout << "\ncalling populateRecords \n";

        for(const auto& object : objects)
        {
            if(object.first.str == fruObjectPath)
            {
                const auto& interfaces = object.second;
                populateRecords(interfaces, recordInfos, entity,fruObjectPath);
                associatedEntityMap.emplace(fruObjectPath, entity);
                break;
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cout << "Config JSONs missing for the item "
                  "interface type, interface = "
                  << fruInterface << "\n";
    }

 //   pldm_entity_association_pdr_remove_contained_entity(pdrRepo,entity,false);
   // pldm_entity_association_pdr_remove_contained_entity(pdrRepo,entity,true);
   // CREATE A SIMILAR API FOR ADD CHILD

   pldm_entity_node* combinedNode;
   pldm_entity parentEntity;
   memcpy(reinterpret_cast<void *>(&parentEntity),reinterpret_cast<void*>(parent),sizeof(pldm_entity));
   pldm_find_entity_ref_in_tree(entityTree, parentEntity, &combinedNode);
   pldm_entity* addEntity = &entity;
   pldm_entity_association_pdr_add_from_node(combinedNode, pdrRepo, &addEntity, 1, true);

   pldm_entity_node* bmcNode;
   pldm_find_entity_ref_in_tree(bmcEntityTree, parentEntity, &bmcNode);
   pldm_entity_association_pdr_add_from_node(bmcNode, pdrRepo, &addEntity, 1, false);

    if (table.size()) //does it work? how does table change?? -- changes automatically in populateRecords
    {
        padBytes = utils::getNumPadBytes(table.size());
        table.resize(table.size() + padBytes, 0);
        // Calculate the checksum
        checksum = crc32(table.data(), table.size());
    }
    std::cout << "\n exit buildIndividualFRU \n";
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

void FruImpl::subscribeFruPresence(const std::string& inventoryObjPath, const std::string& fruInterface, const std::string& itemInterface, std::vector<std::unique_ptr<sdbusplus::bus::match::match>>& fruHotPlugMatch)
{
    std::cout << "entered subscribeFruPresence with " << inventoryObjPath
              << " and " << fruInterface << "\n";
    static constexpr auto MAPPER_BUSNAME = "xyz.openbmc_project.ObjectMapper";
    static constexpr auto MAPPER_PATH = "/xyz/openbmc_project/object_mapper";
    static constexpr auto MAPPER_INTERFACE = "xyz.openbmc_project.ObjectMapper";

    auto& bus = pldm::utils::DBusHandler::getBus();
    try
    {
        std::vector<std::string> fruObjPaths;
        auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                          MAPPER_INTERFACE, "GetSubTreePaths");
        method.append(inventoryObjPath);
        method.append(0);
        method.append(std::vector<std::string>({fruInterface}));
        auto reply = bus.call(method);
        reply.read(fruObjPaths);

        for (const auto& fruObjPath : fruObjPaths)
        {
            std::cout << "subscribing for " << fruObjPath << " and " << fruInterface << "\n";
            using namespace sdbusplus::bus::match::rules;
            fruHotPlugMatch.push_back(
                std::make_unique<sdbusplus::bus::match::match>(
                bus,
                propertiesChanged(fruObjPath,
                                  itemInterface),
                [this,fruObjPath,fruInterface](sdbusplus::message::message& msg){
                    DbusChangedProps props;
                    std::string iface;
                    msg.read(iface, props);
                    std::cout << "inside the lambda \n";
                    processFruPresenceChange(props,fruObjPath,fruInterface);
                }));
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "could not subscribe for hotplug of fru: "
                  << fruInterface << " error " << e.what() << "\n";
    }
    std::cout << "returning from subscribeFruPresence \n";
}

void FruImpl::processFruPresenceChange(const DbusChangedProps& chProperties, const std::string& fruObjPath, const std::string& fruInterface)
{
    std::cout << "enter processFruPresenceChange \n";
    static constexpr auto propertyName = "Present";  //check once
    std::cout << "chProperties.size() " << chProperties.size() << "\n";

    const auto it = chProperties.find(propertyName);
    std::cout << "before if block \n";
    if (it == chProperties.end())
    {
        std::cout << "could not find the property name, returning \n";
        return;
    }
    std::cout << "before getting newPropVal \n";
    auto newPropVal = std::get<bool>(it->second);
   std::cout << "got new property as " << newPropVal << " for fru " 
             << fruObjPath << " and interface " << fruInterface << "\n";
    if(!isBuilt)
    {
        std::cout << "\nfru table is not built yet so no need to process"
                   << " the Presence change now. this will be built automatically"
                   << " with the fru table \n";
        return;
    }
    //#ifdef oem-ibm check here so that we do not build or remove industry
    //std pcie cards

    if(newPropVal)
    {
        std::cout << "handling add calling buildIndividualFRU\n";
        buildIndividualFRU(fruInterface,fruObjPath);
    }
    else
    {
        std::cout << "\nhandle remove fru here \n";
        removeIndividualFRU(fruObjPath);
    }
    //send PDR Repo change event from here 
    std::cout << "\n exit processFruPresenceChange \n";
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
