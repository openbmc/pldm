#include "dbus_impl_requester.hpp"

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
    return pldmInstanceIdDb.next(eid);
}
} // namespace dbus_api
} // namespace pldm
