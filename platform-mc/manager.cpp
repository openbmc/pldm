#include "manager.hpp"

namespace pldm
{
namespace platform_mc
{
exec::task<int> Manager::beforeDiscoverTerminus()
{
    co_return PLDM_SUCCESS;
}

exec::task<int> Manager::afterDiscoverTerminus()
{
    auto rc = co_await platformManager.initTerminus();
    co_return rc;
}

exec::task<int> Manager::pollForPlatformEvent(pldm_tid_t tid)
{
    auto it = termini.find(tid);
    if (it != termini.end())
    {
        auto& terminus = it->second;
        co_await eventManager.pollForPlatformEventTask(tid);
        terminus->pollEvent = false;
    }
    co_return PLDM_SUCCESS;
}

} // namespace platform_mc
} // namespace pldm
