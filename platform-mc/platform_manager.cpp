#include "platform_manager.hpp"

#include "terminus_manager.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

exec::task<int> PlatformManager::initTerminus()
{
    for (auto& [tid, terminus] : termini)
    {
        if (terminus->initialized)
        {
            continue;
        }
        terminus->initialized = true;
    }
    co_return PLDM_SUCCESS;
}

} // namespace platform_mc
} // namespace pldm
