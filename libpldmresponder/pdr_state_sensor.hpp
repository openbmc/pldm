#pragma once

#include "libpldmresponder/pdr_utils.hpp"
#include "utils.hpp"

#include "libpldm/platform.h"

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
template <class Handler>
void generateStateSensorPDR(const Json& json, Handler& handler,
                            pdr_utils::RepoInterface& repo)
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

        pdr->entity_type = e.value("type", 0);
        pdr->entity_instance = e.value("instance", 0);

        // Reserved, entity objects need to be encapsulated

        pdr->terminus_handle = 0;
        pdr->sensor_id = handler.getNextEffecterId();
        pdr->sensor_init = PLDM_NO_INIT;
        pdr->has_description_pdr = false;
        pdr->composite_sensor_count = sensors.size();

        DbusPropertyMaps dbusPropertyMaps{};
        uint8_t* start =
            entry.data() + sizeof(pldm_state_sensor_pdr) - sizeof(uint8_t);
        for (const auto& sensor : sensors)
        {
            auto set = sensor.value("set", empty);
            state_sensor_possible_states* possibleStates =
                reinterpret_cast<state_sensor_possible_states*>(start);
            possibleStates->state_set_id = set.value("id", 0);
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
                stateValues.emplace_back(std::move(state));
            }
            start += possibleStates->possible_states_size;

            DbusMappings dbusMappings{};
            auto dbusEntry = sensor.value("dbus", empty);
            auto objectPath = dbusEntry.value("path", "");
            auto interface = dbusEntry.value("interface", "");
            auto properties = dbusEntry.value("property", empty);
            for (const auto property : properties)
            {
                auto name = property.value("name", "");
                auto type = property.value("type", "");

                pldm::utils::DBusMapping dbusMapping{objectPath, interface,
                                                     name, type};
                dbusMappings.emplace_back(std::move(dbusMapping));
            }

            if (dbusMappings.size() != stateValues.size())
            {
                return;
            }

            StatestoDbusProperty statesToDbusProperty;
            for (size_t i = 0; i < dbusMappings.size(); i++)
            {
                statesToDbusProperty.emplace(std::move(stateValues[i]),
                                             std::move(dbusMappings[i]));
            }
            dbusPropertyMaps.emplace_back(statesToDbusProperty);
        }

        handler.addDbusObjPropertyMaps(pdr->sensor_id, dbusPropertyMaps);
        PdrEntry pdrEntry{};
        pdrEntry.data = entry.data();
        pdrEntry.size = pdrSize;
        repo.addRecord(pdrEntry);

        // Reserved, need to call the addPdrEntityAssociation method
    }
}

} // namespace pdr_state_sensor
} // namespace responder
} // namespace pldm