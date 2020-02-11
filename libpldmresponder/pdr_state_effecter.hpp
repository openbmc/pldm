#pragma once

#include "libpldmresponder/effecters.hpp"
#include "libpldmresponder/pdr_utils.hpp"

#include "libpldm/platform.h"

namespace pldm
{

namespace responder
{

namespace pdr_state_effecter
{

using Json = nlohmann::json;

void generateStateEffecterHandler(const Json& json, pdr_utils::Repo& repo)
{
    static const std::vector<Json> emptyList{};
    static const Json empty{};
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
                std::cerr << "Malformed PDR JSON return "
                             "pdrEntry;- no state set "
                             "info, TYPE="
                          << PLDM_STATE_EFFECTER_PDR << "\n";
                throw InternalFailure();
            }
            pdrSize += sizeof(state_effecter_possible_states) -
                       sizeof(bitfield8_t) + (sizeof(bitfield8_t) * statesSize);
        }
        pdrSize += sizeof(pldm_state_effecter_pdr) - sizeof(uint8_t);

        std::vector<uint8_t> entry{};
        entry.resize(pdrSize);

        pldm_state_effecter_pdr* pdr =
            reinterpret_cast<pldm_state_effecter_pdr*>(entry.data());
        pdr->hdr.record_handle = 0;
        pdr->hdr.version = 1;
        pdr->hdr.type = PLDM_STATE_EFFECTER_PDR;
        pdr->hdr.record_change_num = 0;
        pdr->hdr.length = pdrSize - sizeof(pldm_pdr_hdr);

        pdr->terminus_handle = 0;
        pdr->effecter_id = effecter::nextId();
        pdr->entity_type = e.value("type", 0);
        pdr->entity_instance = e.value("instance", 0);
        pdr->container_id = e.value("container", 0);
        pdr->effecter_semantic_id = 0;
        pdr->effecter_init = PLDM_NO_INIT;
        pdr->has_description_pdr = false;
        pdr->composite_effecter_count = effecters.size();

        using namespace effecter::dbus_mapping;
        DbusObjs dbusObjs{};
        DbusValMaps dbusValMaps{};
        uint8_t* start =
            entry.data() + sizeof(pldm_state_effecter_pdr) - sizeof(uint8_t);
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

            auto dbusEntry = effecter.value("dbus", empty);
            auto objectPath = dbusEntry.value("path", "");
            auto interface = dbusEntry.value("interface", "");
            auto propertyName = dbusEntry.value("property_name", "");
            auto propertyType = dbusEntry.value("property_type", "");
            DBusMapping dbusMapping{objectPath, interface, propertyName,
                                    propertyType};
            dbusObjs.emplace_back(std::move(dbusMapping));

            DbusIdToValMap dbusIdToValMap;
            Json propValues = dbusEntry["property_values"];
            dbusIdToValMap =
                populateMapping(propertyType, propValues, stateValues);
            dbusValMaps.emplace_back(std::move(dbusIdToValMap));
        }
        addDbusObjs(pdr->effecter_id, std::move(dbusObjs));
        addDbusValMaps(pdr->effecter_id, std::move(dbusValMaps));
        PdrEntry pdrEntry{};
        pdrEntry.data = entry.data();
        pdrEntry.size = pdrSize;
        repo.addRecord(pdrEntry);
    }
}

} // namespace pdr_state_effecter
} // namespace responder
} // namespace pldm
