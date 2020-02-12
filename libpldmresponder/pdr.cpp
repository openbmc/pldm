#include "pdr.hpp"

#include "pdr_state_effecter.hpp"

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
    std::map<Type, generateHandler> generateHandlers = {
        {PLDM_STATE_EFFECTER_PDR, [](const auto& json, RepoInterface& repo) {
             pdr_state_effecter::generateStateEffecterHandler(json, repo);
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
                    generateHandlers.at(pdrType)(effecter, repo);
                }
            }
        }
        catch (const InternalFailure& e)
        {
        }
        catch (const Json::exception& e)
        {
            std::cerr << "Failed parsing PDR JSON file, TYPE= " << pdrType
                      << "PATH= " << dirEntry << " ERROR=" << e.what() << "\n";
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
