#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include <map>

#include <fstream>
#include <nlohmann/json.hpp>
#include <functional>

#include "libpldm/platform.h"
#include "effecters.hpp"

//#include <phosphor-logging/elog-errors.hpp>
//#include <phosphor-logging/log.hpp>
//#include <xyz/openbmc_project/Common/error.hpp>

//using namespace phosphor::logging;
//using InternalFailure =
//sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

namespace pldm
{

namespace responder
{

namespace pdr
{

using Type = uint8_t;
using Json = nlohmann::json;
using RecordHandle = uint32_t;
using Entry = std::vector<uint8_t>;
using Pdr = std::vector<Entry>;

class Repo
{
    public:
        virtual void add(Entry&& entry) = 0;
        virtual Entry at(RecordHandle handle) const = 0;
        virtual RecordHandle getNextRecordHandle() const = 0;
        virtual RecordHandle getNextRecordHandle(RecordHandle current) const = 0;
        virtual size_t numEntries() const = 0;
        virtual bool empty() const = 0;
};

namespace internal
{

inline Json readJson(const std::string& dir, Type type)
{
    std::ifstream jsonFile(dir + "/" + std::to_string(type) + ".json");
    if (!jsonFile.is_open())
    {
        //log<level::INFO>("PDR JSON file not found", entry("TYPE=%d", type));
        return {};
    }

    return Json::parse(jsonFile);
}

class IndexedRepo : public Repo
{
    public:
        void add(Entry&& entry)      
        {
            repo.emplace_back(std::move(entry));
        }

        Entry at(RecordHandle handle) const
        {
            return repo.at(handle - 1);
        }

        RecordHandle getNextRecordHandle() const
        {
            return repo.size() + 1;
        }

        RecordHandle getNextRecordHandle(RecordHandle current) const
        {
            return current + 1;
        }

        size_t numEntries() const
        {
            return repo.size();
        }

        bool empty() const
        {
            return repo.empty();
        }

    private:
        Pdr repo{};
};

template<typename T>
void generate(const std::string& dir, T& repo)
{
    using namespace internal;
    std::map<Type, std::function<void(const Json& json, T& repo)>> generators = {
        {
        PLDM_STATE_EFFECTER_PDR,
        [](const auto& json, T& repo) {
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
                        //log<level::ERR>("Malformed PDR JSON - no state set info",
                         //   entry("TYPE=%d", PLDM_STATE_EFFECTER_PDR));
                        //elog<InternalFailure>();
                    }
                    pdrSize += sizeof(state_effecter_possible_states) - sizeof(bitfield8_t) + (sizeof(bitfield8_t) * statesSize);
                }
                pdrSize += sizeof(pldm_state_effecter_pdr) - sizeof(uint8_t);

                Entry pdrEntry{};
                pdrEntry.resize(pdrSize);

                pldm_state_effecter_pdr* pdr = reinterpret_cast<pldm_state_effecter_pdr*>(pdrEntry.data());
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
                uint8_t* start = pdrEntry.data() + sizeof(pldm_state_effecter_pdr) - sizeof(uint8_t);
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

                    auto dbus = effecter.value("dbus", empty);
                    paths.emplace_back(std::move(dbus));
                }
                add(pdr->effecter_id, std::move(paths));
                repo.add(std::move(pdrEntry));
            }
        }
        }
    };

    for (const auto& generator : generators)
    {
        try
        {
            auto json = readJson(dir, generator.first);
            if (!json.empty())
            {
                generator.second(json, repo);
            }
        }
        /*catch (const InternalFailure& e)
        {
        }*/
        catch (const std::exception& e)
        {
            //log<level::ERR>("Failed parsing PDR JSON file",
            //   entry("TYPE=%d", generator.first),
            //  entry("ERROR=%s", e.what()));
            //report<InternalFailure>();
        }
    }
}

} // namespace internal

Repo& get(const std::string& dir);

} // namespace pdr
} // namespace responder
} // namespace pldm
