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
        internal::generate("", repo);
    }

    return repo;
}

} // namespace pdr
} // namespace responder
} // namespace pldm
