#pragma once

#include "libpldm/platform.h"

#include "libpldmresponder/pdr_utils.hpp"

namespace pldm
{

namespace responder
{

namespace pdr_state_sensor
{

using Json = nlohmann::json;

static const Json empty{};

/** @brief Parse PDR JSON file and generate state sensor PDR structure
 *
 *  @param[in] json - the JSON Object with the state sensor PDR
 *  @param[out] handler - the Parser of PLDM command handler
 *  @param[out] repo - pdr::RepoInterface
 *
 */

template <class DBusInterface, class Handler>
void generateStateSensorPDR(const DBusInterface& dBusIntf, const Json& json,
                            Handler& handler, pdr_utils::RepoInterface& repo)
{
    std::cerr << "Inside the generate State sensor PDR function" << std::endl;

    // static auto interFace = "org.freedesktop.DBus.ObjectManager";

    static const std::vector<Json> emptyList{};
    auto entries = json.value("entries", emptyList);
    uint8_t index = 0;
    for (const auto& e : entries)
    {
        index++;
        size_t pdrSize = 0;
        auto sensors = e.value("sensors", emptyList);
        for (const auto& sensor : sensors)
        {
            auto set = sensor.value("set", empty);
            auto statesSize = set.value("size", 0);
            if (!statesSize)
            {
                std::cerr << "Malformed PDR JSON return "
                             "pdrEntry;- no state set "
                             "info, TYPE="
                          << PLDM_STATE_SENSOR_PDR << "\n";
                throw InternalFailure();
            }
            pdrSize += sizeof(state_sensor_possible_states) -
                       sizeof(bitfield8_t) + (sizeof(bitfield8_t) * statesSize);
        }
        pdrSize += sizeof(pldm_state_sensor_pdr) - sizeof(uint8_t);

        std::vector<uint8_t> entry{};
        entry.resize(pdrSize);

        std::cerr << " PDRSIZE : " << pdrSize << std::endl;
        std::cerr << "ENTRY VECTOR SIZE : " << entry.size() << std::endl;

        pldm_state_sensor_pdr* pdr =
            reinterpret_cast<pldm_state_sensor_pdr*>(entry.data());
        if (!pdr)
        {
            std::cerr << "Failed to get state sensor PDR.\n";
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

        pdr->terminus_handle = 0;
        pdr->sensor_id = handler.getNextSensorId();

        try
        {
            std::string entity_path = e.value("entity_path", "");
            auto& associatedEntityMap = handler.getAssociateEntityMap();
            if (entity_path != "" && associatedEntityMap.find(entity_path) !=
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

        bool found = true;
        DbusMappings dbusMappings{};
        DbusValMaps dbusValMaps{};

        uint8_t* start =
            entry.data() + sizeof(pldm_state_sensor_pdr) - sizeof(uint8_t);
        for (const auto& sensor : sensors)
        {
            std::cerr << "Inside the for loop to parse each sensors"
                      << std::endl;

            auto set = sensor.value("set", empty);
            state_sensor_possible_states* possibleStates =
                reinterpret_cast<state_sensor_possible_states*>(start);
            possibleStates->state_set_id = set.value("id", 0);
            HTOLE16(possibleStates->state_set_id);
            possibleStates->possible_states_size = set.value("size", 0);

            start += sizeof(possibleStates->state_set_id) +
                     sizeof(possibleStates->possible_states_size);
            static const std::vector<uint8_t> emptyStates{};
            PossibleValues stateValues;
            auto states = set.value("states", emptyStates);
            for (const auto& state : states)
            {
                auto index = state / 8;
                auto bit = state - (index * 8);
                bitfield8_t* bf = reinterpret_cast<bitfield8_t*>(start + index);
                bf->byte |= 1 << bit;
                stateValues.emplace_back(state);
            }
            start += possibleStates->possible_states_size;
            auto dbusEntry = sensor.value("dbus", empty);
            auto objectPath = dbusEntry.value("path", "");
            auto interface = dbusEntry.value("interface", "");
            auto propertyName = dbusEntry.value("property_name", "");
            auto propertyType = dbusEntry.value("property_type", "");

            StatestoDbusVal dbusIdToValMap{};
            pldm::utils::DBusMapping dbusMapping{};
            try
            {
                std::cerr << "Inside try block" << std::endl;
                auto service =
                    dBusIntf.getService(objectPath.c_str(), interface.c_str());

                dbusMapping = pldm::utils::DBusMapping{
                    objectPath, interface, propertyName, propertyType};
                dbusIdToValMap = populateMapping(
                    propertyType, dbusEntry["property_values"], stateValues);
            }
            catch (const std::exception& e)
            {
                std::cerr << "D-Bus object path does not exist and wait for "
                             "the interface added signal,sensor ID: "
                          << pdr->sensor_id << "\n";

                found = false;
                std::cerr << "Object path inside the catch block: "
                          << objectPath << std::endl;
                std::cerr << "Interface inside the catch block: "
                          << interface << std::endl;
                StatestoDbusVal dbusIdToVal{};

                dbusIdToVal = populateMapping(
                    propertyType, dbusEntry["property_values"], stateValues);

                std::cerr << "Index value : " << (uint16_t)index << std::endl;

                handler.intfSignal.insert(
                    {index,
                     (std::make_unique<sdbusplus::bus::match::match>(
                         pldm::utils::DBusHandler::getBus(),
                         sdbusplus::bus::match::rules::interfacesAdded() +
                             sdbusplus::bus::match::rules::argNpath(0,
                                                                    objectPath),
                         [=, &repo,
                          &handler](sdbusplus::message::message& msg) {
                             pldm::utils::DBusInterfaceAdded interfaces;
                             sdbusplus::message::object_path path;
                             msg.read(path, interfaces);
                             std::cerr << " Inside lambda" << std::endl;
                             std::cerr
                                 << "Interface print: " << interface.c_str()
                                 << std::endl;
                             std::cerr
                                 << "Object Path print: " << objectPath.c_str()
                                 << std::endl;

                             std::cerr << "propertyName print: "
                                       << propertyName.c_str() << std::endl;
                             std::cerr << " property Type print: "
                                       << propertyType.c_str() << std::endl;

                             std::cerr << "Entry size: " << entry.size()
                                       << std::endl;
                             std::cerr << "pdrSize : " << pdrSize << std::endl;

                             std::string iface;
                             std::string opath;
                             for (auto& intf : interfaces)
                             {
                                 std::cerr << " Inside the for loop"
                                           << std::endl;
                                 std::cerr << "Size of interfaces: "
                                           << interfaces.size() << std::endl;
                                 std::cerr << "Intf.first:" << intf.first
                                           << std::endl;

                                 if (intf.first == interface.c_str())
                                 {
                                     std::cerr << "Intf.first:" << intf.first
                                               << std::endl;
                                     std::cerr << "Inside interface check"
                                               << std::endl;
                                     iface = intf.first;
                                     opath = path.str.c_str();
                                     std::cerr << "opath: " << opath
                                               << std::endl;
                                     for (const auto& property : intf.second)
                                     {
                                         std::cerr << "PropertyName:"
                                                   << propertyName.c_str()
                                                   << std::endl;
                                         if (property.first ==
                                             propertyName.c_str())
                                         {
                                             std::cerr
                                                 << "PropertyName inside the "
                                                    "if condition:"
                                                 << propertyName.c_str()
                                                 << std::endl;
                                             break;
                                         }
                                     }

                                     DbusMappings dbusMappings{};
                                     DbusValMaps dbusValMaps{};

                                     pldm::utils::DBusMapping dbusMapping =
                                         pldm::utils::DBusMapping{opath, iface,
                                                                  propertyName,
                                                                  propertyType};

                                     dbusMappings.emplace_back(
                                         std::move(dbusMapping));
                                     dbusValMaps.emplace_back(
                                         std::move(dbusIdToVal));

                                     std::cerr
                                         << " Sensor ID : " << pdr->sensor_id
                                         << std::endl;

                                     handler.addDbusObjMaps(
                                         pdr->sensor_id,
                                         std::make_tuple(
                                             std::move(dbusMappings),
                                             std::move(dbusValMaps)),
                                         TypeId::PLDM_SENSOR_ID);
                                     std::cerr << " Creating the PDR inside "
                                                  "Interface added "
                                                  "signal watcher code"
                                               << std::endl;

                                     PdrEntry pdrEntry{};
                                     pdrEntry.data = reinterpret_cast<uint8_t*>(
                                         const_cast<uint8_t*>(entry.data()));
                                     pdrEntry.size = pdrSize;
                                     repo.addRecord(pdrEntry);
                                     handler.intfSignal.erase(index);

                                     std::cerr
                                         << " After adding it to the repo "
                                         << std::endl;

                                     dbusMappings.clear();
                                     dbusValMaps.clear();

                                     std::cerr << " After clearing"
                                               << std::endl;
                                     break;
                                 }
                             }
                         }))});
            }
            std::cerr << "outside the interface added signal" << std::endl;
            if (found == false)
            {
                std::cerr << " before continue" << std::endl;
                continue;
            }
            else
            {
                std::cerr << " Adding it to the DBus Mapping" << std::endl;
                dbusMappings.emplace_back(std::move(dbusMapping));
                dbusValMaps.emplace_back(std::move(dbusIdToValMap));
            }
        }

        handler.addDbusObjMaps(
            pdr->sensor_id,
            std::make_tuple(std::move(dbusMappings), std::move(dbusValMaps)),
            TypeId::PLDM_SENSOR_ID);

        if (found == true)
        {
            std::cerr << " Creating the PDR " << std::endl;
            PdrEntry pdrEntry{};
            pdrEntry.data = entry.data();
            pdrEntry.size = pdrSize;
            repo.addRecord(pdrEntry);
        }
    }
}

} // namespace pdr_state_sensor
} // namespace responder
} // namespace pldm
