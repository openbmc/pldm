#include "pdr.hpp"

namespace pldm
{

namespace responder
{

namespace pdr
{

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

Repo& getRepoFromType(const std::string& dir, const Type type)
{
    using namespace internal;
    static IndexedRepo pdrRepo;
    pdrRepo.makeEmpty();

    Repo& repo = get(dir);
    uint32_t recordHndl{};
    pdr::Entry pdrEntry{};
    while (true)
    {
        pdrEntry = repo.at(recordHndl);
        pldm_pdr_hdr* header = reinterpret_cast<pldm_pdr_hdr*>(pdrEntry.data());
        if (header->type == type)
        {
            pdrRepo.add(std::move(pdrEntry));
        }

        recordHndl = repo.getNextRecordHandle(recordHndl);
        if (recordHndl)
        {
            continue;
        }

        break;
    }

    return pdrRepo;
}

} // namespace pdr
} // namespace responder
} // namespace pldm
