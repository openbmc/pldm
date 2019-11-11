#include "dbus_impl_requester.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <phosphor-logging/elog-errors.hpp>

using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace phosphor::logging;

namespace pldm
{
namespace dbus_api
{

uint8_t Requester::getInstanceId(uint8_t eid)
{
    if (ids.find(eid) == ids.end())
    {
        InstanceId id;
        ids.emplace(eid, InstanceId());
    }

    uint8_t id{};
    try
    {
        id = ids[eid].next();
    }
    catch (const std::runtime_error& e)
    {
        elog<TooManyResources>();
    }

    return id;
}

} // namespace dbus_api
} // namespace pldm
