#include "pdr.hpp"

namespace pldm
{

namespace responder
{

namespace pdr
{
using namespace pldm::responder::pdrUtils;

void generate(const std::string& dir, Repo& repo)
{
    // A map of PDR type to a lambda that handles creation of that PDR type.
    // The lambda essentially would parse the platform specific PDR JSONs to
    // generate the PDR structures. This function iterates through the map to
    // invoke all lambdas, so that all PDR types can be created.
    std::map<Type, std::function<void(const Json& json, Repo& repo)>>
        generators = {
            {PLDM_STATE_EFFECTER_PDR, [](const auto& json, Repo& repo) {
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
                             std::cerr << "Malformed PDR JSON return "
                                          "pdrEntry;- no state set "
                                          "info, TYPE="
                                       << PLDM_STATE_EFFECTER_PDR << "\n";
                             throw InternalFailure();
                         }
                         pdrSize += sizeof(state_effecter_possible_states) -
                                    sizeof(bitfield8_t) +
                                    (sizeof(bitfield8_t) * statesSize);
                     }
                     pdrSize +=
                         sizeof(pldm_state_effecter_pdr) - sizeof(uint8_t);

                     std::vector<uint8_t> entry{};
                     entry.resize(pdrSize);

                     pldm_state_effecter_pdr* pdr =
                         reinterpret_cast<pldm_state_effecter_pdr*>(
                             entry.data());
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
                     Paths paths{};
                     uint8_t* start = entry.data() +
                                      sizeof(pldm_state_effecter_pdr) -
                                      sizeof(uint8_t);
                     for (const auto& effecter : effecters)
                     {
                         auto set = effecter.value("set", empty);
                         state_effecter_possible_states* possibleStates =
                             reinterpret_cast<state_effecter_possible_states*>(
                                 start);
                         possibleStates->state_set_id = set.value("id", 0);
                         possibleStates->possible_states_size =
                             set.value("size", 0);

                         start += sizeof(possibleStates->state_set_id) +
                                  sizeof(possibleStates->possible_states_size);
                         static const std::vector<uint8_t> emptyStates{};
                         auto states = set.value("states", emptyStates);
                         for (const auto& state : states)
                         {
                             auto index = state / 8;
                             auto bit = state - (index * 8);
                             bitfield8_t* bf =
                                 reinterpret_cast<bitfield8_t*>(start + index);
                             bf->byte |= 1 << bit;
                         }
                         start += possibleStates->possible_states_size;

                         auto dbus = effecter.value("dbus", empty);
                         paths.emplace_back(std::move(dbus));
                     }
                     add(pdr->effecter_id, std::move(paths));
                     PdrEntry pdrEntry{};
                     pdrEntry.data = entry.data();
                     pdrEntry.size = pdrSize;
                     repo.addRecord(pdrEntry);
                 }
             }}};

    Type pdrType{};
    for (const auto& dirEntry : fs::directory_iterator(dir))
    {
        try
        {
            auto json = readJson(dirEntry.path().string());
            if (!json.empty())
            {
                pdrType = json.value("pdrType", 0);
                generators.at(pdrType)(json, repo);
            }
        }
        catch (const InternalFailure& e)
        {
        }
        catch (const Json::exception& e)
        {
            std::cerr << "Failed parsing PDR JSON file, TYPE= " << pdrType
                      << " ERROR=" << e.what() << "\n";
            pldm::utils::reportError(
                "xyz.openbmc_project.bmc.pldm.InternalFailure");
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed parsing PDR JSON file, TYPE= " << pdrType
                      << " ERROR=" << e.what() << "\n";
            pldm::utils::reportError(
                "xyz.openbmc_project.bmc.pldm.InternalFailure");
        }
    }
}

Repo& getRepo(const std::string& dir)
{
    static Repo repo;
    if (repo.empty())
    {
        generate(dir, repo);
    }

    return repo;
}

Repo getRepoByType(const std::string& dir, const Type pdr_type)
{
    Repo pdrRepo;
    Repo& repo = getRepo(dir);

    uint8_t* pdrData = nullptr;
    uint32_t pdrSize{};
    auto record = pldm_pdr_find_record_by_type(repo.getPdr(), pdr_type, NULL,
                                               &pdrData, &pdrSize);
    while (record)
    {
        PdrEntry pdrEntry{};
        pdrEntry.data = pdrData;
        pdrEntry.size = pdrSize;
        pdrEntry.handle.current_handle = repo.getRecordHandle(record);
        pdrRepo.addRecord(pdrEntry);

        pdrData = nullptr;
        pdrSize = 0;
        record = pldm_pdr_find_record_by_type(repo.getPdr(), pdr_type, record,
                                              &pdrData, &pdrSize);
    }

    return pdrRepo;
}

const pldm_pdr_record* getRecordByHandle(Repo& pdrRepo,
                                         const RecordHandle record_handle,
                                         PdrEntry& pdrEntry)
{
    uint8_t* pdrData = nullptr;
    auto record =
        pldm_pdr_find_record(pdrRepo.getPdr(), record_handle, &pdrData,
                             &pdrEntry.size, &pdrEntry.handle.next_handle);
    if (record)
    {
        pdrEntry.data = pdrData;
    }

    return record;
}

} // namespace pdr
} // namespace responder
} // namespace pldm
