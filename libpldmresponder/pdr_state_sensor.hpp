#pragma once

#include "common/types.hpp"
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
                error(
                    "Malformed PDR JSON return pdrEntry; no state set info for state sensor pdr '{STATE_SENSOR_PDR}'",
                    "STATE_SENSOR_PDR", PLDM_STATE_SENSOR_PDR);
                throw InternalFailure();
            }
            pdrSize += PLDM_PLATFORM_STATE_SENSOR_POSSIBLE_STATES_MIN_LENGTH +
                       statesSize;
        }
        pdrSize += PLDM_PLATFORM_STATE_SENSOR_PDR_MIN_LENGTH;

        std::vector<uint8_t> entry(pdrSize, 0);

        struct pldm_platform_state_sensor_pdr pdr{};
        pdr.hdr.record_handle = 0;
        pdr.hdr.version = 1;
        pdr.hdr.type = PLDM_STATE_SENSOR_PDR;
        pdr.hdr.record_change_num = 0;
        pdr.hdr.length = pdrSize - sizeof(pldm_pdr_hdr);

        pdr.terminus_handle = TERMINUS_HANDLE;

        try
        {
            std::string entity_path = e.value("entity_path", "");
            auto& associatedEntityMap = handler.getAssociateEntityMap();
            if (entity_path != "" && associatedEntityMap.contains(entity_path))
            {
                pdr.entity_type =
                    associatedEntityMap.at(entity_path).entity_type;
                pdr.entity_instance_number =
                    associatedEntityMap.at(entity_path).entity_instance_num;
                pdr.container_id =
                    associatedEntityMap.at(entity_path).entity_container_id;
            }
            else
            {
                pdr.entity_type = e.value("type", 0);
                pdr.entity_instance_number = e.value("instance", 0);
                pdr.container_id = e.value("container", 0);

                // do not create the PDR when the FRU or the entity path is not
                // present
                if (!pdr.entity_type)
                {
                    continue;
                }
            }
        }
        catch (const std::exception&)
        {
            pdr.entity_type = e.value("type", 0);
            pdr.entity_instance_number = e.value("instance", 0);
            pdr.container_id = e.value("container", 0);
        }

        pdr.sensor_init = PLDM_NO_INIT;
        pdr.sensor_auxiliary_names_pdr = false;
        if (sensors.size() > 8)
        {
            throw std::runtime_error("sensor size must be less than 8");
        }
        pdr.composite_sensor_count = sensors.size();

        pldm::responder::pdr_utils::DbusMappings dbusMappings{};
        pldm::responder::pdr_utils::DbusValMaps dbusValMaps{};
        size_t offset = PLDM_PLATFORM_STATE_SENSOR_PDR_MIN_LENGTH;
        for (const auto& sensor : sensors)
        {
            auto set = sensor.value("set", empty);
            struct pldm_platform_state_sensor_possible_states possibleStates{};
            possibleStates.state_set_id = set.value("id", 0);
            possibleStates.possible_states_size = set.value("size", 0);

            std::vector<uint8_t> stateBits(possibleStates.possible_states_size,
                                           0);
            static const std::vector<uint8_t> emptyStates{};
            pldm::responder::pdr_utils::PossibleValues stateValues;
            auto states = set.value("states", emptyStates);
            for (const auto& state : states)
            {
                auto index = state / 8;
                if (index >= possibleStates.possible_states_size)
                {
                    error(
                        "Malformed PDR JSON; state '{STATE}' exceeds possible states size '{SIZE}' for state set '{STATE_SET}'",
                        "STATE", state, "SIZE",
                        possibleStates.possible_states_size, "STATE_SET",
                        possibleStates.state_set_id);
                    throw InternalFailure();
                }
                auto bit = state - (index * 8);
                stateBits[index] |= 1 << bit;
                stateValues.emplace_back(state);
            }

            struct variable_field stateField{stateBits.data(),
                                             stateBits.size()};
            size_t encodedSize = entry.size() - offset;
            int rc = encode_pldm_platform_state_sensor_possible_states(
                &possibleStates, &stateField, entry.data() + offset,
                &encodedSize);
            if (rc)
            {
                error(
                    "Failed to encode possible states for state set '{STATE_SET}' of state sensor PDR, error - {RC}",
                    "STATE_SET", possibleStates.state_set_id, "RC", rc);
                throw InternalFailure();
            }
            offset += encodedSize;

            auto dbusEntry = sensor.value("dbus", empty);
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
                error(
                    "Failed to create sensor PDR, D-Bus object '{PATH}' returned error - {ERROR}",
                    "PATH", objectPath, "ERROR", e);
                break;
            }
            dbusMappings.emplace_back(std::move(dbusMapping));
            dbusValMaps.emplace_back(std::move(dbusIdToValMap));
        }

        if (!(dbusMappings.empty() && dbusValMaps.empty()))
        {
            pdr.sensor_id = handler.getNextSensorId();
            size_t encodedSize = PLDM_PLATFORM_STATE_SENSOR_PDR_MIN_LENGTH;
            int rc = encode_pldm_platform_state_sensor_pdr(&pdr, entry.data(),
                                                           &encodedSize);
            if (rc)
            {
                error(
                    "Failed to encode state sensor PDR for sensor ID '{SENSOR_ID}', error - {RC}",
                    "SENSOR_ID", pdr.sensor_id, "RC", rc);
                continue;
            }
            handler.addDbusObjMaps(
                pdr.sensor_id,
                std::make_tuple(std::move(dbusMappings),
                                std::move(dbusValMaps)),
                pldm::responder::pdr_utils::TypeId::PLDM_SENSOR_ID);
            pldm::responder::pdr_utils::PdrEntry pdrEntry{};
            pdrEntry.data = entry.data();
            pdrEntry.size = pdrSize;
            repo.addRecord(pdrEntry);
        }
    }
}

} // namespace pdr_state_sensor
} // namespace responder
} // namespace pldm
