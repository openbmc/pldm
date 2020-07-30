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

        pldm_state_sensor_pdr* pdr =
            reinterpret_cast<pldm_state_sensor_pdr*>(entry.data());
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
        pdr->entity_type = e.value("type", 0);
        pdr->entity_instance = e.value("instance", 0);
        pdr->container_id = e.value("container", 0);
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

        DbusMappings dbusMappings{};
        DbusValMaps dbusValMaps{};
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

            try
            {
                auto service =
                    dBusIntf.getService(objectPath.c_str(), interface.c_str());
            }
            catch (const std::exception& e)
            {
                continue;
            }

            pldm::utils::DBusMapping dbusMapping{objectPath, interface,
                                                 propertyName, propertyType};
            dbusMappings.emplace_back(std::move(dbusMapping));

            Json propValues = dbusEntry["property_values"];
            StatestoDbusVal dbusIdToValMap =
                populateMapping(propertyType, propValues, stateValues);
            if (!dbusIdToValMap.empty())
            {
                dbusValMaps.emplace_back(std::move(dbusIdToValMap));
            }
        }

        handler.addDbusObjMaps(
            pdr->sensor_id,
            std::make_tuple(std::move(dbusMappings), std::move(dbusValMaps)),
            TypeId::PLDM_SENSOR_ID);
        PdrEntry pdrEntry{};
        pdrEntry.data = entry.data();
        pdrEntry.size = pdrSize;
        repo.addRecord(pdrEntry);
    }
}

} // namespace pdr_state_sensor
} // namespace responder
} // namespace pldm
