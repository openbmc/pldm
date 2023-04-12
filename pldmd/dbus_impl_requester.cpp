#include "dbus_impl_requester.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

using namespace sdbusplus::xyz::openbmc_project::Common::Error;

namespace pldm
{
namespace dbus_api
{

/* Ideally we would be able to look up the TID for a given EID. We don't
 * have that infrastructure in place yet. So use the EID value for the TID.
 * This is an interim step towards the PLDM requester logic moving into
 * libpldm, and eventually this won't be needed. */

uint8_t Requester::getInstanceId(uint8_t eid)
{
    uint8_t id;
    int rc = pldm_instance_id_alloc(pldmInstID, eid, &id);
    if (rc == -EAGAIN)
    {
        throw TooManyResources();
    }
    else if (rc)
    {
        throw std::exception();
    }
    return id;
}
} // namespace dbus_api
} // namespace pldm
