#include "pdr.hpp"

#include "libpldmresponder/pdr_state_effecter.hpp"

using namespace pldm::responder::pdrUtils;

namespace pldm
{

namespace responder
{

namespace pdr
{

void generate(const std::string& dir, Repo& repo)
{
    // A map of PDR type to a lambda that handles creation of that PDR type.
    // The lambda essentially would parse the platform specific PDR JSONs to
    // generate the PDR structures. This function iterates through the map to
    // invoke all lambdas, so that all PDR types can be created.
    std::map<Type, generateHandler> generateHandlers = {
        {PLDM_STATE_EFFECTER_PDR, [](const auto& json, Repo& repo) {
             pdrStateEffecter::generateStateEffecterHandler(json, repo);
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
                generateHandlers.at(pdrType)(json, repo);
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
