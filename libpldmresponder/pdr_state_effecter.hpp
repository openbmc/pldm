#pragma once

#include "libpldm/platform.h"

#include "pdr.hpp"
#include "pdr_utils.hpp"

#include <config.h>

namespace pldm
{

namespace responder
{

namespace pdr_state_effecter
{

using Json = nlohmann::json;

static const Json empty{};

/** @brief Parse PDR JSON file and generate state effecter PDR structure
 *
 *  @param[in] json - the JSON Object with the state effecter PDR
 *  @param[out] handler - the Parser of PLDM command handler
 *  @param[out] repo - pdr::RepoInterface
 *
 */
template <class DBusInterface, class Handler>
void generateStateEffecterPDR(const DBusInterface& dBusIntf, const Json& json,
                              Handler& handler, pdr_utils::RepoInterface& repo)
{
    std::cout << "\nenter generateStateEffecterPDR" << std::endl;
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
        if (!pdr)
        {
            std::cerr << "Failed to get state effecter PDR.\n";
            continue;
        }
        pdr->hdr.record_handle = 0;
        pdr->hdr.version = 1;
        pdr->hdr.type = PLDM_STATE_EFFECTER_PDR;
        pdr->hdr.record_change_num = 0;
        pdr->hdr.length = pdrSize - sizeof(pldm_pdr_hdr);

        pdr->terminus_handle = TERMINUS_HANDLE;
        pdr->effecter_id = handler.getNextEffecterId();

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

        pdr->effecter_semantic_id = 0;
        pdr->effecter_init = PLDM_NO_INIT;
        pdr->has_description_pdr = false;
        pdr->composite_effecter_count = effecters.size();

        pldm::responder::pdr_utils::DbusMappings dbusMappings{};
        pldm::responder::pdr_utils::DbusValMaps dbusValMaps{};
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
            pldm::responder::pdr_utils::PossibleValues stateValues;
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
                    dBusIntf.getService(objectPath.c_str(), interface.c_str());

                dbusMapping = pldm::utils::DBusMapping{
                    objectPath, interface, propertyName, propertyType};
                dbusIdToValMap = pldm::responder::pdr_utils::populateMapping(
                    propertyType, dbusEntry["property_values"], stateValues);
            }
            catch (const std::exception& e)
            {
                std::cerr << "D-Bus object path does not exist, effecter ID: "
                          << pdr->effecter_id << "\n";
            }

            dbusMappings.emplace_back(std::move(dbusMapping));
            dbusValMaps.emplace_back(std::move(dbusIdToValMap));
        }
        handler.addDbusObjMaps(
            pdr->effecter_id,
            std::make_tuple(std::move(dbusMappings), std::move(dbusValMaps)));
        pldm::responder::pdr_utils::PdrEntry pdrEntry{};
        pdrEntry.data = entry.data();
        pdrEntry.size = pdrSize;
        repo.addRecord(pdrEntry);
    }
    std::cout << "\nexit generateStateEffecterPDR" << std::endl;
}

} // namespace pdr_state_effecter
} // namespace responder
} // namespace pldm
