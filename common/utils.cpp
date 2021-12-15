#include "utils.hpp"

#include <libpldm/pdr.h>
#include <libpldm/pldm_types.h>

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Logging/Create/client.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace utils
{

using ObjectMapper = sdbusplus::client::xyz::openbmc_project::ObjectMapper<>;

Entities getParentEntites(const EntityAssociations& entityAssoc)
{
    Entities parents{};
    for (const auto& et : entityAssoc)
    {
        parents.push_back(et[0]);
    }

    bool found = false;
    for (auto it = parents.begin(); it != parents.end();
         it = found ? parents.erase(it) : std::next(it))
    {
        uint16_t parent_contained_id =
            pldm_entity_node_get_remote_container_id(*it);
        found = false;
        for (const auto& evs : entityAssoc)
        {
            for (size_t i = 1; i < evs.size() && !found; i++)
            {
                uint16_t node_contained_id =
                    pldm_entity_node_get_remote_container_id(evs[i]);

                pldm_entity parent_entity = pldm_entity_extract(*it);
                pldm_entity node_entity = pldm_entity_extract(evs[i]);

                if (node_entity.entity_type == parent_entity.entity_type &&
                    node_entity.entity_instance_num ==
                        parent_entity.entity_instance_num &&
                    node_contained_id == parent_contained_id)
                {
                    found = true;
                }
            }
            if (found)
            {
                break;
            }
        }
    }

    return parents;
}

void addObjectPathEntityAssociations(const EntityAssociations& entityAssoc,
                                     pldm_entity_node* entity,
                                     const fs::path& path,
                                     ObjectPathMaps& objPathMap)
{
    if (entity == nullptr)
    {
        return;
    }

    bool found = false;
    pldm_entity node_entity = pldm_entity_extract(entity);
    if (!entityMaps.contains(node_entity.entity_type))
    {
        lg2::info(
            "{ENTITY_TYPE} Entity fetched from remote PLDM terminal does not exist.",
            "ENTITY_TYPE", (int)node_entity.entity_type);
        return;
    }

    std::string entityName = entityMaps.at(node_entity.entity_type);
    for (const auto& ev : entityAssoc)
    {
        pldm_entity ev_entity = pldm_entity_extract(ev[0]);
        if (ev_entity.entity_instance_num == node_entity.entity_instance_num &&
            ev_entity.entity_type == node_entity.entity_type)
        {
            uint16_t node_contained_id =
                pldm_entity_node_get_remote_container_id(ev[0]);
            uint16_t entity_contained_id =
                pldm_entity_node_get_remote_container_id(entity);

            if (node_contained_id != entity_contained_id)
            {
                continue;
            }

            fs::path p = path / fs::path{entityName +
                                         std::to_string(
                                             node_entity.entity_instance_num)};
            std::string entity_path = p.string();
            // If the entity obtained from the remote PLDM terminal is not in
            // the MAP, or there is no auxiliary name PDR, add it directly.
            // Otherwise, check whether the DBus service of entity_path exists,
            // and overwrite the entity if it does not exist.
            if (!objPathMap.contains(entity_path))
            {
                objPathMap[entity_path] = entity;
            }
            else
            {
                try
                {
                    pldm::utils::DBusHandler().getService(entity_path.c_str(),
                                                          nullptr);
                }
                catch (const std::exception& e)
                {
                    objPathMap[entity_path] = entity;
                }
            }

            for (size_t i = 1; i < ev.size(); i++)
            {
                addObjectPathEntityAssociations(entityAssoc, ev[i], p,
                                                objPathMap);
            }
            found = true;
        }
    }

    if (!found)
    {
        std::string dbusPath =
            path / fs::path{entityName +
                            std::to_string(node_entity.entity_instance_num)};

        try
        {
            pldm::utils::DBusHandler().getService(dbusPath.c_str(), nullptr);
        }
        catch (const std::exception& e)
        {
            objPathMap[dbusPath] = entity;
        }
    }
}

void updateEntityAssociation(const EntityAssociations& entityAssoc,
                             pldm_entity_association_tree* entityTree,
                             ObjectPathMaps& objPathMap)
{
    std::vector<pldm_entity_node*> parentsEntity =
        getParentEntites(entityAssoc);
    for (const auto& entity : parentsEntity)
    {
        fs::path path{"/xyz/openbmc_project/inventory"};
        std::deque<std::string> paths{};
        pldm_entity node_entity = pldm_entity_extract(entity);
        auto node = pldm_entity_association_tree_find_with_locality(
            entityTree, &node_entity, false);
        if (!node)
        {
            continue;
        }

        bool found = true;
        while (node)
        {
            if (!pldm_entity_is_exist_parent(node))
            {
                break;
            }

            pldm_entity parent = pldm_entity_get_parent(node);
            try
            {
                paths.push_back(entityMaps.at(parent.entity_type) +
                                std::to_string(parent.entity_instance_num));
            }
            catch (const std::exception& e)
            {
                lg2::error(
                    "Parent entity not found in the entityMaps, type: {ENTITY_TYPE}, num: {NUM}, e: {ERROR}",
                    "ENTITY_TYPE", (int)parent.entity_type, "NUM",
                    (int)parent.entity_instance_num, "ERROR", e);
                found = false;
                break;
            }

            node = pldm_entity_association_tree_find_with_locality(
                entityTree, &parent, false);
        }

        if (!found)
        {
            continue;
        }

        while (!paths.empty())
        {
            path = path / fs::path{paths.back()};
            paths.pop_back();
        }

        addObjectPathEntityAssociations(entityAssoc, entity, path, objPathMap);
    }
}

std::vector<std::vector<uint8_t>> findStateEffecterPDR(uint8_t /*tid*/,
                                                       uint16_t entityID,
                                                       uint16_t stateSetId,
                                                       const pldm_pdr* repo)
{
    uint8_t* outData = nullptr;
    uint32_t size{};
    const pldm_pdr_record* record{};
    std::vector<std::vector<uint8_t>> pdrs;
    try
    {
        do
        {
            record = pldm_pdr_find_record_by_type(repo, PLDM_STATE_EFFECTER_PDR,
                                                  record, &outData, &size);
            if (record)
            {
                auto pdr = reinterpret_cast<pldm_state_effecter_pdr*>(outData);
                auto compositeEffecterCount = pdr->composite_effecter_count;
                auto possible_states_start = pdr->possible_states;

                for (auto effecters = 0x00; effecters < compositeEffecterCount;
                     effecters++)
                {
                    auto possibleStates =
                        reinterpret_cast<state_effecter_possible_states*>(
                            possible_states_start);
                    auto setId = possibleStates->state_set_id;
                    auto possibleStateSize =
                        possibleStates->possible_states_size;

                    if (pdr->entity_type == entityID && setId == stateSetId)
                    {
                        std::vector<uint8_t> effecter_pdr(&outData[0],
                                                          &outData[size]);
                        pdrs.emplace_back(std::move(effecter_pdr));
                        break;
                    }
                    possible_states_start += possibleStateSize + sizeof(setId) +
                                             sizeof(possibleStateSize);
                }
            }

        } while (record);
    }
    catch (const std::exception& e)
    {
        error(" Failed to obtain a record. ERROR = {ERR_EXCEP}", "ERR_EXCEP",
              e.what());
    }

    return pdrs;
}

std::vector<std::vector<uint8_t>> findStateSensorPDR(uint8_t /*tid*/,
                                                     uint16_t entityID,
                                                     uint16_t stateSetId,
                                                     const pldm_pdr* repo)
{
    uint8_t* outData = nullptr;
    uint32_t size{};
    const pldm_pdr_record* record{};
    std::vector<std::vector<uint8_t>> pdrs;
    try
    {
        do
        {
            record = pldm_pdr_find_record_by_type(repo, PLDM_STATE_SENSOR_PDR,
                                                  record, &outData, &size);
            if (record)
            {
                auto pdr = reinterpret_cast<pldm_state_sensor_pdr*>(outData);
                auto compositeSensorCount = pdr->composite_sensor_count;
                auto possible_states_start = pdr->possible_states;

                for (auto sensors = 0x00; sensors < compositeSensorCount;
                     sensors++)
                {
                    auto possibleStates =
                        reinterpret_cast<state_sensor_possible_states*>(
                            possible_states_start);
                    auto setId = possibleStates->state_set_id;
                    auto possibleStateSize =
                        possibleStates->possible_states_size;

                    if (pdr->entity_type == entityID && setId == stateSetId)
                    {
                        std::vector<uint8_t> sensor_pdr(&outData[0],
                                                        &outData[size]);
                        pdrs.emplace_back(std::move(sensor_pdr));
                        break;
                    }
                    possible_states_start += possibleStateSize + sizeof(setId) +
                                             sizeof(possibleStateSize);
                }
            }

        } while (record);
    }
    catch (const std::exception& e)
    {
        error(" Failed to obtain a record. ERROR = {ERR_EXCEP}", "ERR_EXCEP",
              e.what());
    }

    return pdrs;
}

uint8_t readHostEID()
{
    uint8_t eid{};
    std::ifstream eidFile{HOST_EID_PATH};
    if (!eidFile.good())
    {
        error("Could not open host EID file: {HOST_PATH}", "HOST_PATH",
              static_cast<std::string>(HOST_EID_PATH));
    }
    else
    {
        std::string eidStr;
        eidFile >> eidStr;
        if (!eidStr.empty())
        {
            eid = atoi(eidStr.c_str());
        }
        else
        {
            error("Host EID file was empty");
        }
    }

    return eid;
}

uint8_t getNumPadBytes(uint32_t data)
{
    uint8_t pad;
    pad = ((data % 4) ? (4 - data % 4) : 0);
    return pad;
} // end getNumPadBytes

bool uintToDate(uint64_t data, uint16_t* year, uint8_t* month, uint8_t* day,
                uint8_t* hour, uint8_t* min, uint8_t* sec)
{
    constexpr uint64_t max_data = 29991231115959;
    constexpr uint64_t min_data = 19700101000000;
    if (data < min_data || data > max_data)
    {
        return false;
    }

    *year = data / 10000000000;
    data = data % 10000000000;
    *month = data / 100000000;
    data = data % 100000000;
    *day = data / 1000000;
    data = data % 1000000;
    *hour = data / 10000;
    data = data % 10000;
    *min = data / 100;
    *sec = data % 100;

    return true;
}

std::optional<std::vector<set_effecter_state_field>>
    parseEffecterData(const std::vector<uint8_t>& effecterData,
                      uint8_t effecterCount)
{
    std::vector<set_effecter_state_field> stateField;

    if (effecterData.size() != effecterCount * 2)
    {
        return std::nullopt;
    }

    for (uint8_t i = 0; i < effecterCount; ++i)
    {
        uint8_t set_request = effecterData[i * 2] == PLDM_REQUEST_SET
                                  ? PLDM_REQUEST_SET
                                  : PLDM_NO_CHANGE;
        set_effecter_state_field filed{set_request, effecterData[i * 2 + 1]};
        stateField.emplace_back(std::move(filed));
    }

    return std::make_optional(std::move(stateField));
}

std::string DBusHandler::getService(const char* path,
                                    const char* interface) const
{
    using DbusInterfaceList = std::vector<std::string>;
    std::map<std::string, std::vector<std::string>> mapperResponse;
    auto& bus = DBusHandler::getBus();

    auto mapper = bus.new_method_call(ObjectMapper::default_service,
                                      ObjectMapper::instance_path,
                                      ObjectMapper::interface, "GetObject");

    if (interface)
    {
        mapper.append(path, DbusInterfaceList({interface}));
    }
    else
    {
        mapper.append(path, DbusInterfaceList({}));
    }

    auto mapperResponseMsg = bus.call(mapper, dbusTimeout);
    mapperResponseMsg.read(mapperResponse);
    return mapperResponse.begin()->first;
}

GetSubTreeResponse
    DBusHandler::getSubtree(const std::string& searchPath, int depth,
                            const std::vector<std::string>& ifaceList) const
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    auto method = bus.new_method_call(ObjectMapper::default_service,
                                      ObjectMapper::instance_path,
                                      ObjectMapper::interface, "GetSubTree");
    method.append(searchPath, depth, ifaceList);
    auto reply = bus.call(method, dbusTimeout);
    GetSubTreeResponse response;
    reply.read(response);
    return response;
}

void reportError(const char* errorMsg, const Severity& sev)
{
    auto& bus = pldm::utils::DBusHandler::getBus();

    try
    {
        using LoggingCreate =
            sdbusplus::client::xyz::openbmc_project::logging::Create<>;

        std::string severity = "xyz.openbmc_project.Logging.Entry.Level.Error";

        using namespace sdbusplus::xyz::openbmc_project::Logging::server;
        auto method = bus.new_method_call(LoggingCreate::default_service,
                                          LoggingCreate::instance_path,
                                          LoggingCreate::interface, "Create");

        try
        {
            if (sevMap.contains(sev))
            {
                severity = sevMap.at(sev);
            }
        }
        catch (const std::out_of_range& e)
        {
            error("Severity not found : {ERROR}", "ERROR", e);
        }

        std::map<std::string, std::string> addlData{};
        method.append(errorMsg, severity, addlData);
        bus.call_noreply(method, dbusTimeout);
    }
    catch (const std::exception& e)
    {
        error(
            "failed to make a d-bus call to create error log, ERROR={ERR_EXCEP}",
            "ERR_EXCEP", e.what());
    }
}

void DBusHandler::setDbusProperty(const DBusMapping& dBusMap,
                                  const PropertyValue& value) const
{
    auto setDbusValue = [&dBusMap, this](const auto& variant) {
        auto& bus = getBus();
        auto service = getService(dBusMap.objectPath.c_str(),
                                  dBusMap.interface.c_str());
        auto method = bus.new_method_call(
            service.c_str(), dBusMap.objectPath.c_str(), dbusProperties, "Set");
        method.append(dBusMap.interface.c_str(), dBusMap.propertyName.c_str(),
                      variant);
        bus.call_noreply(method, dbusTimeout);
    };

    if (dBusMap.propertyType == "uint8_t")
    {
        std::variant<uint8_t> v = std::get<uint8_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "bool")
    {
        std::variant<bool> v = std::get<bool>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "int16_t")
    {
        std::variant<int16_t> v = std::get<int16_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "uint16_t")
    {
        std::variant<uint16_t> v = std::get<uint16_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "int32_t")
    {
        std::variant<int32_t> v = std::get<int32_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "uint32_t")
    {
        std::variant<uint32_t> v = std::get<uint32_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "int64_t")
    {
        std::variant<int64_t> v = std::get<int64_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "uint64_t")
    {
        std::variant<uint64_t> v = std::get<uint64_t>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "double")
    {
        std::variant<double> v = std::get<double>(value);
        setDbusValue(v);
    }
    else if (dBusMap.propertyType == "string")
    {
        std::variant<std::string> v = std::get<std::string>(value);
        setDbusValue(v);
    }
    else
    {
        throw std::invalid_argument("UnSpported Dbus Type");
    }
}

PropertyValue DBusHandler::getDbusPropertyVariant(
    const char* objPath, const char* dbusProp, const char* dbusInterface) const
{
    auto& bus = DBusHandler::getBus();
    auto service = getService(objPath, dbusInterface);
    auto method = bus.new_method_call(service.c_str(), objPath, dbusProperties,
                                      "Get");
    method.append(dbusInterface, dbusProp);
    return bus.call(method, dbusTimeout).unpack<PropertyValue>();
}

PropertyValue jsonEntryToDbusVal(std::string_view type,
                                 const nlohmann::json& value)
{
    PropertyValue propValue{};
    if (type == "uint8_t")
    {
        propValue = static_cast<uint8_t>(value);
    }
    else if (type == "uint16_t")
    {
        propValue = static_cast<uint16_t>(value);
    }
    else if (type == "uint32_t")
    {
        propValue = static_cast<uint32_t>(value);
    }
    else if (type == "uint64_t")
    {
        propValue = static_cast<uint64_t>(value);
    }
    else if (type == "int16_t")
    {
        propValue = static_cast<int16_t>(value);
    }
    else if (type == "int32_t")
    {
        propValue = static_cast<int32_t>(value);
    }
    else if (type == "int64_t")
    {
        propValue = static_cast<int64_t>(value);
    }
    else if (type == "bool")
    {
        propValue = static_cast<bool>(value);
    }
    else if (type == "double")
    {
        propValue = static_cast<double>(value);
    }
    else if (type == "string")
    {
        propValue = static_cast<std::string>(value);
    }
    else
    {
        error("Unknown D-Bus property type, TYPE={OTHER_TYPE}", "OTHER_TYPE",
              type);
    }

    return propValue;
}

uint16_t findStateEffecterId(const pldm_pdr* pdrRepo, uint16_t entityType,
                             uint16_t entityInstance, uint16_t containerId,
                             uint16_t stateSetId, bool localOrRemote)
{
    uint8_t* pdrData = nullptr;
    uint32_t pdrSize{};
    const pldm_pdr_record* record{};
    do
    {
        record = pldm_pdr_find_record_by_type(pdrRepo, PLDM_STATE_EFFECTER_PDR,
                                              record, &pdrData, &pdrSize);
        if (record && (localOrRemote ^ pldm_pdr_record_is_remote(record)))
        {
            auto pdr = reinterpret_cast<pldm_state_effecter_pdr*>(pdrData);
            auto compositeEffecterCount = pdr->composite_effecter_count;
            auto possible_states_start = pdr->possible_states;

            for (auto effecters = 0x00; effecters < compositeEffecterCount;
                 effecters++)
            {
                auto possibleStates =
                    reinterpret_cast<state_effecter_possible_states*>(
                        possible_states_start);
                auto setId = possibleStates->state_set_id;
                auto possibleStateSize = possibleStates->possible_states_size;

                if (entityType == pdr->entity_type &&
                    entityInstance == pdr->entity_instance &&
                    containerId == pdr->container_id && stateSetId == setId)
                {
                    return pdr->effecter_id;
                }
                possible_states_start += possibleStateSize + sizeof(setId) +
                                         sizeof(possibleStateSize);
            }
        }
    } while (record);

    return PLDM_INVALID_EFFECTER_ID;
}

int emitStateSensorEventSignal(uint8_t tid, uint16_t sensorId,
                               uint8_t sensorOffset, uint8_t eventState,
                               uint8_t previousEventState)
{
    try
    {
        auto& bus = DBusHandler::getBus();
        auto msg = bus.new_signal("/xyz/openbmc_project/pldm",
                                  "xyz.openbmc_project.PLDM.Event",
                                  "StateSensorEvent");
        msg.append(tid, sensorId, sensorOffset, eventState, previousEventState);

        msg.signal_send();
    }
    catch (const std::exception& e)
    {
        error("Error emitting pldm event signal:ERROR={ERR_EXCEP}", "ERR_EXCEP",
              e.what());
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

uint16_t findStateSensorId(const pldm_pdr* pdrRepo, uint8_t tid,
                           uint16_t entityType, uint16_t entityInstance,
                           uint16_t containerId, uint16_t stateSetId)
{
    auto pdrs = findStateSensorPDR(tid, entityType, stateSetId, pdrRepo);
    for (auto pdr : pdrs)
    {
        auto sensorPdr = reinterpret_cast<pldm_state_sensor_pdr*>(pdr.data());
        auto compositeSensorCount = sensorPdr->composite_sensor_count;
        auto possible_states_start = sensorPdr->possible_states;

        for (auto sensors = 0x00; sensors < compositeSensorCount; sensors++)
        {
            auto possibleStates =
                reinterpret_cast<state_sensor_possible_states*>(
                    possible_states_start);
            auto setId = possibleStates->state_set_id;
            auto possibleStateSize = possibleStates->possible_states_size;
            if (entityType == sensorPdr->entity_type &&
                entityInstance == sensorPdr->entity_instance &&
                stateSetId == setId && containerId == sensorPdr->container_id)
            {
                return sensorPdr->sensor_id;
            }
            possible_states_start += possibleStateSize + sizeof(setId) +
                                     sizeof(possibleStateSize);
        }
    }
    return PLDM_INVALID_EFFECTER_ID;
}

void printBuffer(bool isTx, const std::vector<uint8_t>& buffer)
{
    if (!buffer.empty())
    {
        if (isTx)
        {
            std::cout << "Tx: ";
        }
        else
        {
            std::cout << "Rx: ";
        }
        std::ostringstream tempStream;
        for (int byte : buffer)
        {
            tempStream << std::setfill('0') << std::setw(2) << std::hex << byte
                       << " ";
        }
        std::cout << tempStream.str() << std::endl;
    }
}

std::string toString(const struct variable_field& var)
{
    if (var.ptr == nullptr || !var.length)
    {
        return "";
    }

    std::string str(reinterpret_cast<const char*>(var.ptr), var.length);
    std::replace_if(
        str.begin(), str.end(), [](const char& c) { return !isprint(c); }, ' ');
    return str;
}

std::vector<std::string> split(std::string_view srcStr, std::string_view delim,
                               std::string_view trimStr)
{
    std::vector<std::string> out;
    size_t start;
    size_t end = 0;

    while ((start = srcStr.find_first_not_of(delim, end)) != std::string::npos)
    {
        end = srcStr.find(delim, start);
        std::string_view dstStr = srcStr.substr(start, end - start);
        if (!trimStr.empty())
        {
            dstStr.remove_prefix(dstStr.find_first_not_of(trimStr));
            dstStr.remove_suffix(dstStr.size() - 1 -
                                 dstStr.find_last_not_of(trimStr));
        }

        if (!dstStr.empty())
        {
            out.push_back(std::string(dstStr));
        }
    }

    return out;
}

std::string getCurrentSystemTime()
{
    using namespace std::chrono;
    const time_point<system_clock> tp = system_clock::now();
    std::time_t tt = system_clock::to_time_t(tp);
    auto ms = duration_cast<microseconds>(tp.time_since_epoch()) -
              duration_cast<seconds>(tp.time_since_epoch());

    std::stringstream ss;
    ss << std::put_time(std::localtime(&tt), "%F %Z %T.")
       << std::to_string(ms.count());
    return ss.str();
}

bool checkForFruPresence(const std::string& objPath)
{
    bool isPresent = false;
    static constexpr auto presentInterface =
        "xyz.openbmc_project.Inventory.Item";
    static constexpr auto presentProperty = "Present";
    try
    {
        auto propVal = pldm::utils::DBusHandler().getDbusPropertyVariant(
            objPath.c_str(), presentProperty, presentInterface);
        isPresent = std::get<bool>(propVal);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        error(
            "Failed to check for FRU presence for {OBJ_PATH} ERROR = {ERR_EXCEP}",
            "OBJ_PATH", objPath.c_str(), "ERR_EXCEP", e.what());
    }
    return isPresent;
}

bool checkIfLogicalBitSet(const uint16_t& containerId)
{
    return !(containerId & 0x8000);
}

void setFruPresence(const std::string& fruObjPath, bool present)
{
    pldm::utils::PropertyValue value{present};
    pldm::utils::DBusMapping dbusMapping;
    dbusMapping.objectPath = fruObjPath;
    dbusMapping.interface = "xyz.openbmc_project.Inventory.Item";
    dbusMapping.propertyName = "Present";
    dbusMapping.propertyType = "bool";
    try
    {
        pldm::utils::DBusHandler().setDbusProperty(dbusMapping, value);
    }
    catch (const std::exception& e)
    {
        error(
            "Failed to set the present property on path: '{PATH}' with {ERROR} ",
            "PATH", fruObjPath, "ERROR", e);
    }
}

} // namespace utils
} // namespace pldm
