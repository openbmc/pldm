#include "manager.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

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

} // namespace platform_mc
} // namespace pldm
