#include "pdr.hpp"

namespace pldm
{

namespace responder
{

namespace pdr
{
using namespace pldm::responder::pdr_utils;

void generate(const std::string& dir, RepoInterface& repo)
{
    // A map of PDR type to a lambda that handles creation of that PDR type.
    // The lambda essentially would parse the platform specific PDR JSONs to
    // generate the PDR structures. This function iterates through the map to
    // invoke all lambdas, so that all PDR types can be created.
    std::map<Type, std::function<void(const Json& json, RepoInterface& repo)>>
        generators = {
            {PLDM_STATE_EFFECTER_PDR,
             [](const auto& json, RepoInterface& repo) {
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
                     DbusObjs dbusObjs{};
                     DbusValMaps dbusValMaps{};
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
                         PossibleValues stateValues;
                         auto states = set.value("states", emptyStates);
                         for (const auto& state : states)
                         {
                             auto index = state / 8;
                             auto bit = state - (index * 8);
                             bitfield8_t* bf =
                                 reinterpret_cast<bitfield8_t*>(start + index);
                             bf->byte |= 1 << bit;
                             stateValues.emplace_back(std::move(state));
                         }
                         start += possibleStates->possible_states_size;

                         auto dbusEntry = effecter.value("dbus", empty);
                         auto objectPath = dbusEntry.value("path", "");
                         auto interface = dbusEntry.value("interface", "");
                         auto propertyName =
                             dbusEntry.value("property_name", "");
                         auto propertyType =
                             dbusEntry.value("property_type", "");
                         DBusMapping dbusMapping{objectPath, interface,
                                                 propertyName, propertyType};
                         dbusObjs.emplace_back(std::move(dbusMapping));

                         DbusIdToValMap dbusIdToValMap;
                         Json propValues = dbusEntry["property_values"];
                         dbusIdToValMap = populateMapping(
                             propertyType, propValues, stateValues);
                         dbusValMaps.emplace_back(std::move(dbusIdToValMap));
                     }
                     addDbusObjs(pdr->effecter_id, std::move(dbusObjs));
                     addDbusValMaps(pdr->effecter_id, std::move(dbusValMaps));
                     PdrEntry pdrEntry{};
                     pdrEntry.data = entry.data();
                     pdrEntry.size = pdrSize;
                     repo.addRecord(pdrEntry);
                 }
             }}};

    Type pdrType{};
    static const Json empty{};
    for (const auto& dirEntry : fs::directory_iterator(dir))
    {
        try
        {
            auto json = readJson(dirEntry.path().string());
            if (!json.empty())
            {
                auto effecterPDRs = json.value("effecterPDRs", empty);
                for (const auto& effecter : effecterPDRs)
                {
                    pdrType = effecter.value("pdrType", 0);
                    generators.at(pdrType)(effecter, repo);
                }
            }
        }
        catch (const InternalFailure& e)
        {
            std::cerr << "PDR config directory does not exist or empty, TYPE= "
                      << pdrType << "PATH= " << dirEntry
                      << " ERROR=" << e.what() << "\n";
        }
        catch (const Json::exception& e)
        {
            std::cerr << "Failed parsing PDR JSON file, TYPE= " << pdrType
                      << "PATH= " << dirEntry << " ERROR=" << e.what() << "\n";
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

RepoInterface& getRepo(const std::string& dir)
{
    static Repo repo;
    if (repo.empty())
    {
        generate(dir, repo);
    }

    return repo;
}

Repo getRepoByType(const std::string& dir, Type pdrType)
{
    Repo pdrRepo;
    RepoInterface& repo = getRepo(dir);

    uint8_t* pdrData = nullptr;
    uint32_t pdrSize{};
    auto record = pldm_pdr_find_record_by_type(repo.getPdr(), pdrType, NULL,
                                               &pdrData, &pdrSize);
    while (record)
    {
        PdrEntry pdrEntry{};
        pdrEntry.data = pdrData;
        pdrEntry.size = pdrSize;
        pdrEntry.handle.recordHandle = repo.getRecordHandle(record);
        pdrRepo.addRecord(pdrEntry);

        pdrData = nullptr;
        pdrSize = 0;
        record = pldm_pdr_find_record_by_type(repo.getPdr(), pdrType, record,
                                              &pdrData, &pdrSize);
    }

    return pdrRepo;
}

const pldm_pdr_record* getRecordByHandle(RepoInterface& pdrRepo,
                                         RecordHandle recordHandle,
                                         PdrEntry& pdrEntry)
{
    uint8_t* pdrData = nullptr;
    auto record =
        pldm_pdr_find_record(pdrRepo.getPdr(), recordHandle, &pdrData,
                             &pdrEntry.size, &pdrEntry.handle.nextRecordHandle);
    if (record)
    {
        pdrEntry.data = pdrData;
    }

    return record;
}

} // namespace pdr
} // namespace responder
} // namespace pldm
