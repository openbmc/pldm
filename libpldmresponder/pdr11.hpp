#pragma once

#include "effecters.hpp"

#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>
#include <xyz/openbmc_project/Common/error.hpp>

#include "libpldm/platform.h"

using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

namespace pldm
{

namespace responder
{

namespace pdr11
{

using Json = nlohmann::json;
using Entry = std::vector<uint8_t>;

template <typename T>
void generate_pldm_state_effecter_pdr(const Json& json, T& repo)
{
    static const std::vector<Json> emptyList{};
    static const Json empty{};
    auto entries = json.value("entries", emptyList);
    for (const auto& e : entries)
    {
        size_t pdrSize = 0;
        auto effecters = e.value("effecters", emptyList);
        static const Json empty{};
        for (const auto& effecter : effecters)
        {
            auto set = effecter.value("set", empty);
            auto statesSize = set.value("size", 0);
            if (!statesSize)
            {
                std::cerr << "Malformed PDR JSON - no state set info, TYPE="
                          << PLDM_STATE_EFFECTER_PDR << "\n";
                throw InternalFailure();
            }
            pdrSize += sizeof(state_effecter_possible_states) -
                       sizeof(bitfield8_t) + (sizeof(bitfield8_t) * statesSize);
        }
        pdrSize += sizeof(pldm_state_effecter_pdr) - sizeof(uint8_t);

        Entry pdrEntry{};
        pdrEntry.resize(pdrSize);

        pldm_state_effecter_pdr* pdr =
            reinterpret_cast<pldm_state_effecter_pdr*>(pdrEntry.data());
        pdr->hdr.record_handle = repo.getNextRecordHandle();
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
        Paths paths{};
        uint8_t* start =
            pdrEntry.data() + sizeof(pldm_state_effecter_pdr) - sizeof(uint8_t);
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
            auto states = set.value("states", emptyStates);
            for (const auto& state : states)
            {
                auto index = state / 8;
                auto bit = state - (index * 8);
                bitfield8_t* bf = reinterpret_cast<bitfield8_t*>(start + index);
                bf->byte |= 1 << bit;
            }
            start += possibleStates->possible_states_size;

            auto dbus = effecter.value("dbus", empty);
            paths.emplace_back(std::move(dbus));
        }
        add(pdr->effecter_id, std::move(paths));
        repo.add(std::move(pdrEntry));
    }
}

} // namespace pdr11
} // namespace responder
} // namespace pldm
