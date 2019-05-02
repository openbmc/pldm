#include "pdr_manager.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <functional>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

using namespace phosphor::logging;
using InternalFailure =
sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

namespace pldm
{

namespace responder
{

namespace pdr
{

using Type = uint8_t;
using Json = nlohmann::json;

namespace internal
{
    std::map<RecordHandle, Entry> repo{};

    Json readJson(const std::string& dir, Type type)
    {
        std::ifstream jsonFile(dir + "/" + std::to_string(type));
        if (!jsonFile.is_open())
        {
            log<level::ERR>("PDR JSON file not found", entry("TYPE=%d", type));
            elog<InternalFailure>();
        }

        return Json::parse(jsonFile);
    }
} // namespace internal

void generatePDR(const std::string& dir, std::map<RecordHandle, Entry>& pdrRepo)
{
    using namespace internal;
    static RecordHandle handle = 0;

    static std::map<Type, std::function<void(const Json& json)>> generators = {
        {
        PLDM_STATE_EFFECTER_PDR,
        [](const auto& json) {
            static const std::vector<Json> emptyList{};
            static const Json empty{};
            auto entries = json.value("entries", emptyList);
            size_t pdrSize = 0;
            for (const auto& e : entries)
            {
                auto effecters = e.value("effecters", emptyList);
                static const Json empty{};
                for (const auto& effecter : effecters)
                {
                    auto set = effecter.value("set", empty);
                    auto statesSize = set.value("size", 0);
                    if (!statesSize)
                    {
                        log<level::ERR>("Malformed PDR JSON - no state set info",
                            entry("TYPE=%d", PLDM_STATE_EFFECTER_PDR));
                        elog<InternalFailure>();
                    }
                    pdrSize += sizeof(state_effecter_possible_states) - sizeof(bitfield8_t) + (sizeof(bitfield8_t) * statesSize);
                }
                pdrSize += sizeof(pldm_pdr_hdr) + sizeof(pldm_state_effecter_pdr) - sizeof(uint8_t);

                Entry pdrEntry{};
                pdrEntry.resize(pdrSize);

                pldm_pdr_hdr* hdr = reinterpret_cast<pldm_pdr_hdr*>(pdrEntry.data());
                hdr->record_handle = ++handle;
                hdr->version = 1;
                hdr->type = PLDM_STATE_EFFECTER_PDR;
                hdr->record_change_num = 0;
                hdr->length = pdrSize - sizeof(pldm_pdr_hdr);

                pldm_state_effecter_pdr* pdr = reinterpret_cast<pldm_state_effecter_pdr*>(pdrEntry.data() + sizeof(pldm_pdr_hdr));
                pdr->terminus_handle = 0;
                pdr->effecter_id = e.value("id", 0);
                pdr->entity_type = e.value("type", 0);
                pdr->entity_instance = e.value("instance", 0);
                pdr->container_id = e.value("container", 0);
                pdr->effecter_semantic_id = 0;
                pdr->effecter_init = PLDM_NO_INIT;
                pdr->has_description_pdr = false;
                pdr->composite_effecter_count = effecters.size();

                uint8_t* start = pdrEntry.data() + sizeof(pldm_pdr_hdr) + sizeof(pldm_state_effecter_pdr) - sizeof(uint8_t);
                for (const auto& effecter : effecters)
                {
                    auto set = effecter.value("set", empty);
                    state_effecter_possible_states* possibleStates = reinterpret_cast<state_effecter_possible_states*>(start);
                    possibleStates->state_set_id = set.value("id", 0);
                    possibleStates->possible_states_size = set.value("size", 0);

                    start += sizeof(possibleStates->state_set_id) + sizeof(possibleStates->possible_states_size);
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
                }
            }
            // custom code for this Type
        }
        }
    };

    if (repo.empty())
    {
        for (const auto& generator : generators)
        {
            try
            {
                auto json = readJson(dir, generator.first);
                generator.second(json);
            }
            catch (const InternalFailure& e)
            {
                commit<InternalFailure>();
            }
            catch (const std::exception& e)
            {
                log<level::ERR>("Failed parsing PDR JSON file",
                    entry("TYPE=%d", generator.first),
                    entry("ERROR=%s", e.what()));
                report<InternalFailure>();
            }
        }
    }
}

} // namespace pdr
} // namespace responder
} // namespace pldm
