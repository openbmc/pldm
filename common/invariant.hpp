#pragma once

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace pldm
{

// Keep PLDM's fail-fast checks active even when NDEBUG disables diagnostic
// assertions in dependencies.
inline void requireInvariant(
    bool condition, const char* description,
    const std::source_location location = std::source_location::current())
{
    if (!condition)
    {
        std::fprintf(stderr, "%s:%u: PLDM invariant failed: %s\n",
                     location.file_name(),
                     static_cast<unsigned int>(location.line()), description);
        std::abort();
    }
}

} // namespace pldm
