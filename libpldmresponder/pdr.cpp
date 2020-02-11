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

void getRepoByType(const std::string& dir, Type type, Repo& repo)
{
    using namespace internal;

    repo.makeEmpty();
    IndexedRepo pdrRepo;
    generate(dir, pdrRepo);
    uint32_t recordHandle = 1;
    pdr::Entry pdrEntry{};
    while (true)
    {
        pdrEntry = pdrRepo.at(recordHandle);
        pldm_pdr_hdr* header = reinterpret_cast<pldm_pdr_hdr*>(pdrEntry.data());
        if (header->type == type)
        {
            repo.add(std::move(pdrEntry));
        }

        recordHandle = pdrRepo.getNextRecordHandle(recordHandle);
        if (!recordHandle)
        {
            break;
        }
    }
}

} // namespace pdr
} // namespace responder
} // namespace pldm
