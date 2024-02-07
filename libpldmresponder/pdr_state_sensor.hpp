#pragma once

#include "libpldmresponder/pdr_utils.hpp"

#include <libpldm/platform.h>

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace responder
{
namespace pdr_state_sensor
{
using Json = nlohmann::json;

static const Json empty{};

constexpr uint32_t BMC_PDR_START_RANGE = 0x00000000;
constexpr uint32_t BMC_PDR_END_RANGE = 0x00FFFFFF;

/** @brief Parse PDR JSON file and generate state sensor PDR structure
 *
 *  @param[in] json - the JSON Object with the state sensor PDR
 *  @param[out] handler - the Parser of PLDM command handler
 *  @param[out] repo - pdr::RepoInterface
 *
 */
template <class DBusInterface, class Handler>
void generateStateSensorPDR(const DBusInterface& dBusIntf, const Json& json,
                            Handler& handler, pdr_utils::RepoInterface& repo,
                            pldm_entity_association_tree* bmcEntityTree)
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
                error(
                    "Malformed PDR JSON return pdrEntry;- no state set info, TYPE={STATE_SENSOR_PDR}",
                    "STATE_SENSOR_PDR",
                    static_cast<int>(PLDM_STATE_SENSOR_PDR));
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
        if (!pdr)
        {
            error("Failed to get state sensor PDR.");
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

        pdr->terminus_handle = TERMINUS_HANDLE;
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

                // do not create the PDR when the FRU or the entity path is not
                // present

                if (!pdr->entity_type)
                {
                    error("The entity path for the FRU is not present.");
                    continue;
                }

                // now attach this entity to the container that was
                // mentioned in the json, and add this entity to the
                // parents entity assocation PDR

                std::string parent_entity_path = e.value("parent_entity_path",
                                                         "");
                if (parent_entity_path != "" &&
                    associatedEntityMap.contains(parent_entity_path))
                {
                    // find the parent node in the tree
                    pldm_entity parent_entity =
                        associatedEntityMap.at(parent_entity_path);
                    pldm_entity child_entity = {pdr->entity_type,
                                                pdr->entity_instance,
                                                pdr->container_id};
                    uint8_t bmcEventDataOps = PLDM_INVALID_OP;
                    auto parent_node = pldm_entity_association_tree_find_with_locality(
                        bmcEntityTree, &parent_entity, false);
                    if (!parent_node)
                    {
                        // parent node not found in the entity association tree,
                        // this should not be possible
                        error(
                            "Parent Entity of type {P_ENTITY_TYP} not found in the BMC Entity Association tree",
                            "P_ENTITY_TYP",
                            static_cast<unsigned>(parent_entity.entity_type));
                        return;
                    }
                    pldm_entity_association_tree_add_entity(
                        bmcEntityTree, &child_entity, pdr->entity_instance,
                        parent_node, PLDM_ENTITY_ASSOCIAION_PHYSICAL, false,
                        false, 0xFFFF);
                    uint32_t bmc_record_handle = 0;
#ifdef OEM_IBM
                    auto lastInRangeRecord =
                        pldm_pdr_find_last_in_range(repo.getPdr(), BMC_PDR_START_RANGE, BMC_PDR_END_RANGE);
                    bmc_record_handle = lastInRangeRecord->record_handle;
#endif

                    pldm_entity_association_pdr_add_contained_entity(
                        repo.getPdr(), child_entity, parent_entity,
                        &bmcEventDataOps, false, bmc_record_handle);
                }
            }
        }
        catch (const std::exception&)
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

        pldm::responder::pdr_utils::DbusMappings dbusMappings{};
        pldm::responder::pdr_utils::DbusValMaps dbusValMaps{};
        uint8_t* start = entry.data() + sizeof(pldm_state_sensor_pdr) -
                         sizeof(uint8_t);
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
            auto dbusEntry = sensor.value("dbus", empty);
            auto objectPath = dbusEntry.value("path", "");
            auto interface = dbusEntry.value("interface", "");
            auto propertyName = dbusEntry.value("property_name", "");
            auto propertyType = dbusEntry.value("property_type", "");

            pldm::responder::pdr_utils::StatestoDbusVal dbusIdToValMap{};
            pldm::utils::DBusMapping dbusMapping{};
            try
            {
                auto service = dBusIntf.getService(objectPath.c_str(),
                                                   interface.c_str());

                dbusMapping = pldm::utils::DBusMapping{
                    objectPath, interface, propertyName, propertyType};
                dbusIdToValMap = pldm::responder::pdr_utils::populateMapping(
                    propertyType, dbusEntry["property_values"], stateValues);
            }
            catch (const std::exception& e)
            {
                error(
                    "D-Bus object path does not exist, sensor ID: {SENSOR_ID}",
                    "SENSOR_ID", static_cast<uint16_t>(pdr->sensor_id));
            }

            dbusMappings.emplace_back(std::move(dbusMapping));
            dbusValMaps.emplace_back(std::move(dbusIdToValMap));
        }

        handler.addDbusObjMaps(
            pdr->sensor_id,
            std::make_tuple(std::move(dbusMappings), std::move(dbusValMaps)),
            pldm::responder::pdr_utils::TypeId::PLDM_SENSOR_ID);
        pldm::responder::pdr_utils::PdrEntry pdrEntry{};
        pdrEntry.data = entry.data();
        pdrEntry.size = pdrSize;
        repo.addRecord(pdrEntry);
    }
}

} // namespace pdr_state_sensor
} // namespace responder
} // namespace pldm
