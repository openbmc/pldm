#pragma once

#include "libpldm/platform.h"

#include "libpldmresponder/pdr_utils.hpp"

namespace pldm
{

namespace responder
{

namespace pdr_numeric_effecter
{

using Json = nlohmann::json;

static const Json empty{};

/** @brief Parse PDR JSON file and generate numeric effecter PDR structure
 *
 *  @param[in] json - the JSON Object with the numeric effecter PDR
 *  @param[out] handler - the Parser of PLDM command handler
 *  @param[out] repo - pdr::RepoInterface
 *
 */
template <class DBusInterface, class Handler>
void generateNumericEffecterPDR(const DBusInterface& dBusIntf, const Json& json,
                                Handler& handler,
                                pdr_utils::RepoInterface& repo)
{
    static const std::vector<Json> emptyList{};
    auto entries = json.value("entries", emptyList);
    auto index = 0;
    for (const auto& e : entries)
    {
        index++;
        std::vector<uint8_t> entry{};
        entry.resize(sizeof(pldm_numeric_effecter_value_pdr));

        pldm_numeric_effecter_value_pdr* pdr =
            reinterpret_cast<pldm_numeric_effecter_value_pdr*>(entry.data());
        if (!pdr)
        {
            std::cerr << "Failed to get numeric effecter PDR.\n";
            continue;
        }
        pdr->hdr.record_handle = 0;
        pdr->hdr.version = 1;
        pdr->hdr.type = PLDM_NUMERIC_EFFECTER_PDR;
        pdr->hdr.record_change_num = 0;
        pdr->hdr.length =
            sizeof(pldm_numeric_effecter_value_pdr) - sizeof(pldm_pdr_hdr);

        pdr->terminus_handle = e.value("terminus_handle", 0);
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

        pdr->effecter_semantic_id = e.value("effecter_semantic_id", 0);
        pdr->effecter_init = e.value("effecter_init", PLDM_NO_INIT);
        pdr->effecter_auxiliary_names = e.value("effecter_init", false);
        pdr->base_unit = e.value("base_unit", 0);
        pdr->unit_modifier = e.value("unit_modifier", 0);
        pdr->rate_unit = e.value("rate_unit", 0);
        pdr->base_oem_unit_handle = e.value("base_oem_unit_handle", 0);
        pdr->aux_unit = e.value("aux_unit", 0);
        pdr->aux_unit_modifier = e.value("aux_unit_modifier", 0);
        pdr->aux_oem_unit_handle = e.value("aux_oem_unit_handle", 0);
        pdr->aux_rate_unit = e.value("aux_rate_unit", 0);
        pdr->is_linear = e.value("is_linear", true);
        pdr->effecter_data_size =
            e.value("effecter_data_size", PLDM_EFFECTER_DATA_SIZE_UINT8);
        pdr->resolution = e.value("effecter_resolution_init", 1.00);
        pdr->offset = e.value("offset", 0.00);
        pdr->accuracy = e.value("accuracy", 0);
        pdr->plus_tolerance = e.value("plus_tolerance", 0);
        pdr->minus_tolerance = e.value("minus_tolerance", 0);
        pdr->state_transition_interval =
            e.value("state_transition_interval", 0.00);
        pdr->transition_interval = e.value("transition_interval", 0.00);
        switch (pdr->effecter_data_size)
        {
            case PLDM_EFFECTER_DATA_SIZE_UINT8:
                pdr->max_set_table.value_u8 = e.value("max_set_table", 0);
                pdr->min_set_table.value_u8 = e.value("min_set_table", 0);
                break;
            case PLDM_EFFECTER_DATA_SIZE_SINT8:
                pdr->max_set_table.value_s8 = e.value("max_set_table", 0);
                pdr->min_set_table.value_s8 = e.value("min_set_table", 0);
                break;
            case PLDM_EFFECTER_DATA_SIZE_UINT16:
                pdr->max_set_table.value_u16 = e.value("max_set_table", 0);
                pdr->min_set_table.value_u16 = e.value("min_set_table", 0);
                break;
            case PLDM_EFFECTER_DATA_SIZE_SINT16:
                pdr->max_set_table.value_s16 = e.value("max_set_table", 0);
                pdr->min_set_table.value_s16 = e.value("min_set_table", 0);
                break;
            case PLDM_EFFECTER_DATA_SIZE_UINT32:
                pdr->max_set_table.value_u32 = e.value("max_set_table", 0);
                pdr->min_set_table.value_u32 = e.value("min_set_table", 0);
                break;
            case PLDM_EFFECTER_DATA_SIZE_SINT32:
                pdr->max_set_table.value_s32 = e.value("max_set_table", 0);
                pdr->min_set_table.value_s32 = e.value("min_set_table", 0);
                break;
            default:
                break;
        }

        pdr->range_field_format =
            e.value("range_field_format", PLDM_RANGE_FIELD_FORMAT_UINT8);
        pdr->range_field_support.byte = e.value("range_field_support", 0);
        switch (pdr->range_field_format)
        {
            case PLDM_RANGE_FIELD_FORMAT_UINT8:
                pdr->nominal_value.value_u8 = e.value("nominal_value", 0);
                pdr->normal_max.value_u8 = e.value("normal_max", 0);
                pdr->normal_min.value_u8 = e.value("normal_min", 0);
                pdr->rated_max.value_u8 = e.value("rated_max", 0);
                pdr->rated_min.value_u8 = e.value("rated_min", 0);
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT8:
                pdr->nominal_value.value_s8 = e.value("nominal_value", 0);
                pdr->normal_max.value_s8 = e.value("normal_max", 0);
                pdr->normal_min.value_s8 = e.value("normal_min", 0);
                pdr->rated_max.value_s8 = e.value("rated_max", 0);
                pdr->rated_min.value_s8 = e.value("rated_min", 0);
                break;
            case PLDM_RANGE_FIELD_FORMAT_UINT16:
                pdr->nominal_value.value_u16 = e.value("nominal_value", 0);
                pdr->normal_max.value_u16 = e.value("normal_max", 0);
                pdr->normal_min.value_u16 = e.value("normal_min", 0);
                pdr->rated_max.value_u16 = e.value("rated_max", 0);
                pdr->rated_min.value_u16 = e.value("rated_min", 0);
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT16:
                pdr->nominal_value.value_s16 = e.value("nominal_value", 0);
                pdr->normal_max.value_s16 = e.value("normal_max", 0);
                pdr->normal_min.value_s16 = e.value("normal_min", 0);
                pdr->rated_max.value_s16 = e.value("rated_max", 0);
                pdr->rated_min.value_s16 = e.value("rated_min", 0);
                break;
            case PLDM_RANGE_FIELD_FORMAT_UINT32:
                pdr->nominal_value.value_u32 = e.value("nominal_value", 0);
                pdr->normal_max.value_u32 = e.value("normal_max", 0);
                pdr->normal_min.value_u32 = e.value("normal_min", 0);
                pdr->rated_max.value_u32 = e.value("rated_max", 0);
                pdr->rated_min.value_u32 = e.value("rated_min", 0);
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT32:
                pdr->nominal_value.value_s32 = e.value("nominal_value", 0);
                pdr->normal_max.value_s32 = e.value("normal_max", 0);
                pdr->normal_min.value_s32 = e.value("normal_min", 0);
                pdr->rated_max.value_s32 = e.value("rated_max", 0);
                pdr->rated_min.value_s32 = e.value("rated_min", 0);
                break;
            case PLDM_RANGE_FIELD_FORMAT_REAL32:
                pdr->nominal_value.value_f32 = e.value("nominal_value", 0);
                pdr->normal_max.value_f32 = e.value("normal_max", 0);
                pdr->normal_min.value_f32 = e.value("normal_min", 0);
                pdr->rated_max.value_f32 = e.value("rated_max", 0);
                pdr->rated_min.value_f32 = e.value("rated_min", 0);
                break;
            default:
                break;
        }

        auto dbusEntry = e.value("dbus", empty);
        auto objectPath = dbusEntry.value("path", "");
        auto interface = dbusEntry.value("interface", "");
        auto propertyName = dbusEntry.value("property_name", "");
        auto propertyType = dbusEntry.value("property_type", "");

        pldm::responder::pdr_utils::DbusMappings dbusMappings{};
        pldm::responder::pdr_utils::DbusValMaps dbusValMaps{};
        bool found = true;
        pldm::utils::DBusMapping dbusMapping{};
        try
        {
            auto service =
                dBusIntf.getService(objectPath.c_str(), interface.c_str());

            dbusMapping = pldm::utils::DBusMapping{objectPath, interface,
                                                   propertyName, propertyType};
        }
        catch (const std::exception& e)
        {
            std::cerr << "D-Bus object path does not exist and wait for the "
                         "interface added signal, effecter ID: "
                      << pdr->effecter_id << "\n";

            found = false;
            handler.MatchPointers[2].emplace(
                index,
                (std::make_unique<sdbusplus::bus::match::match>(
                    pldm::utils::DBusHandler::getBus(),
                    sdbusplus::bus::match::rules::interfacesAdded() +
                        sdbusplus::bus::match::rules::argNpath(0, objectPath),
                    [=, &repo, &handler](sdbusplus::message::message& msg) {
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
                                    if (property.first == propertyName.c_str())
                                    {
                                        break;
                                    }
                                }
                                pldm::responder::pdr_utils::DbusMappings
                                    dbusMappings{};
                                pldm::responder::pdr_utils::DbusValMaps
                                    dbusValMaps{};
                                pldm::utils::DBusMapping dbusMapping =
                                    pldm::utils::DBusMapping{opath, iface,
                                                             propertyName,
                                                             propertyType};
                                dbusMappings.emplace_back(
                                    std::move(dbusMapping));
                                handler.addDbusObjMaps(
                                    pdr->effecter_id,
                                    std::make_tuple(std::move(dbusMappings),
                                                    std::move(dbusValMaps)),
                                    pldm::responder::pdr_utils::TypeId::
                                        PLDM_EFFECTER_ID);

                                pldm::responder::pdr_utils::PdrEntry pdrEntry{};
                                pdrEntry.data = reinterpret_cast<uint8_t*>(
                                    const_cast<uint8_t*>(entry.data()));
                                pdrEntry.size =
                                    sizeof(pldm_numeric_effecter_value_pdr);
                                repo.addRecord(pdrEntry);
                                handler.MatchPointers[2].erase(index);
                                break;
                            }
                        }
                    })));
        }
        if (found == false)
        {
            // adding a dummy entry
            dbusMapping = pldm::utils::DBusMapping{"xyz", interface,
                                                   propertyName, propertyType};
            dbusMappings.emplace_back(std::move(dbusMapping));
            continue;
        }
        else
        {
            dbusMappings.emplace_back(std::move(dbusMapping));
        }

        handler.addDbusObjMaps(
            pdr->effecter_id,
            std::make_tuple(std::move(dbusMappings), std::move(dbusValMaps)));

        if (found == true)
        {
            pdr_utils::PdrEntry pdrEntry{};
            pdrEntry.data = entry.data();
            pdrEntry.size = sizeof(pldm_numeric_effecter_value_pdr);
            repo.addRecord(pdrEntry);
        }
    }
}

} // namespace pdr_numeric_effecter
} // namespace responder
} // namespace pldm
