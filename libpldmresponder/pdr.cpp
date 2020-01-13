#include "pdr.hpp"

namespace pldm
{

namespace responder
{

namespace pdr
{

namespace internal
{
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

template <typename T>
void generate_pldm_numeric_effecter_pdr(const Json& json, T& repo)
{
    static const std::vector<Json> emptyList{};
    static const Json empty{};
    auto entries = json.value("entries", emptyList);
    for (const auto& e : entries)
    {
        Entry pdrEntry{};
        pdrEntry.resize(sizeof(pldm_numeric_effecter_valuer_pdr));

        pldm_numeric_effecter_valuer_pdr* pdr =
            reinterpret_cast<pldm_numeric_effecter_valuer_pdr*>(
                pdrEntry.data());
        pdr->hdr.record_handle = repo.getNextRecordHandle();
        pdr->hdr.version = 1;
        pdr->hdr.type = PLDM_NUMERIC_EFFECTER_PDR;
        pdr->hdr.record_change_num = 0;
        pdr->hdr.length =
            sizeof(pldm_numeric_effecter_valuer_pdr) - sizeof(pldm_pdr_hdr);

        pdr->terminus_handle = e.value("terminus_handle", 0);
        pdr->effecter_id = e.value("effecter_id", 0);
        pdr->entity_type = e.value("entity_type", 0);
        pdr->entity_instance = e.value("entity_instance", 0);
        pdr->container_id = e.value("container_id", 0);
        pdr->effecter_semantic_id = e.value("effecter_semantic_id", 0);
        pdr->effecter_init = e.value("effecter_init", PLDM_NO_INIT);
        pdr->has_description_pdr = e.value("effecter_init", false);
        pdr->base_unit = e.value("base_unit", 0);
        pdr->unit_modifier = e.value("unit_modifier", 0);
        pdr->rate_unit = e.value("rate_unit", 0);
        pdr->base_oem_unit_handle = e.value("base_oem_unit_handle", 0);
        pdr->aux_unit = e.value("aux_unit", 0);
        pdr->aux_unit_modifier = e.value("aux_unit_modifier", 0);
        pdr->aux_rate_unit = e.value("aux_rate_unit", 0);
        pdr->is_linear = e.value("is_linear", true);
        pdr->effecter_data_size =
            e.value("effecter_data_size", PLDM_EFFECTER_DATA_SIZE_UINT8);
        pdr->resolution = e.value("effecter_resolutioninit", 0.00);
        pdr->offset = e.value("offset", 0.00);
        pdr->accuracy = e.value("accuracy", 0);
        pdr->plus_to_lerance = e.value("plus_to_lerance", 0);
        pdr->minus_to_lerance = e.value("minus_to_lerance", 0);
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

        using namespace effecter::dbus_mapping;
        Paths paths{};
        auto dbus = e.value("dbus", empty);
        paths.emplace_back(std::move(dbus));
        add(pdr->effecter_id, std::move(paths));
        repo.add(std::move(pdrEntry));
    }
}

} // namespace internal

Repo& get(const std::string& dir)
{
    using namespace internal;
    static IndexedRepo repo;
    if (repo.empty())
    {
        generate(dir, repo);
    }

    return repo;
}

} // namespace pdr
} // namespace responder
} // namespace pldm
