#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
PHOSPHOR_LOG2_USING;

namespace pldm::oem_meta
{

namespace
{

/* @brief map PLDM TID to MCTP EID; currently, they have a one-to-one
 *        correspondence
 */
eid mapTIDtoEID(pldm_tid_t tid)
{
    return static_cast<eid>(tid);
}

} // namespace

bool checkMetaIana(pldm_tid_t tid)
{
    [[maybe_unused]] eid EID = mapTIDtoEID(tid);
    return true;
}

std::string getSlotNumberStringByTID(pldm_tid_t tid)
{
    [[maybe_unused]] eid EID = mapTIDtoEID(tid);
    return std::string{"0"};
}

} // namespace pldm::oem_meta
