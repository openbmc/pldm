#pragma once

#include "libpldm/platform.h"

#include "pdr.hpp"
#include "pdr_utils.hpp"

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
    static const std::vector<Json> emptyList{};
    auto entries = json.value("entries", emptyList);
    uint8_t index = 0;
    for (const auto& e : entries)
    {
        index++;
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

        pdr->terminus_handle = pdr::BmcPldmTerminusHandle;
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

        bool found = true;
        uint8_t compEffecterCount = pdr->composite_effecter_count;
        DbusMappings dbusMappings{};
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
                stateValues.emplace_back(state);
            }
            start += possibleStates->possible_states_size;

            auto dbusEntry = effecter.value("dbus", empty);
            auto objectPath = dbusEntry.value("path", "");
            auto interface = dbusEntry.value("interface", "");
            auto propertyName = dbusEntry.value("property_name", "");
            auto propertyType = dbusEntry.value("property_type", "");

            StatestoDbusVal dbusIdToValMap{};
            pldm::utils::DBusMapping dbusMapping{};
            try
            {
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
                             "the interface added signal, effecter ID: "
                          << pdr->effecter_id << "\n";

                found = false;
                compEffecterCount--;
                StatestoDbusVal dbusIdToVal{};
                dbusIdToVal = populateMapping(
                    propertyType, dbusEntry["property_values"], stateValues);
                handler.intfEffecSignal.insert(
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
                             std::string iface;
                             std::string opath;
                             for (auto& intf : interfaces)
                             {
                                 if (intf.first == interface.c_str())
                                 {
                                     iface = intf.first;
                                     opath = path.str.c_str();
                                     for (const auto& property : intf.second)
                                     {
                                         if (property.first ==
                                             propertyName.c_str())
                                         {
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
                                     handler.addDbusObjMaps(
                                         pdr->effecter_id,
                                         std::make_tuple(
                                             std::move(dbusMappings),
                                             std::move(dbusValMaps)),
                                         TypeId::PLDM_EFFECTER_ID);

                                     PdrEntry pdrEntry{};
                                     pdrEntry.data = reinterpret_cast<uint8_t*>(
                                         const_cast<uint8_t*>(entry.data()));
                                     pdrEntry.size = pdrSize;
                                     repo.addRecord(pdrEntry);
                                     handler.intfEffecSignal.erase(index);
                                     break;
                                 }
                             }
                         }))});
            }
            if (found == false)
            {
                continue;
            }
            else
            {
                dbusMappings.emplace_back(std::move(dbusMapping));
                dbusValMaps.emplace_back(std::move(dbusIdToValMap));
            }
        }

        handler.addDbusObjMaps(
            pdr->effecter_id,
            std::make_tuple(std::move(dbusMappings), std::move(dbusValMaps)));
        if (found == true || compEffecterCount != 0)
        {
            PdrEntry pdrEntry{};
            pdrEntry.data = entry.data();
            pdrEntry.size = pdrSize;
            repo.addRecord(pdrEntry);
        }
    }
}

} // namespace pdr_state_effecter
} // namespace responder
} // namespace pldm
