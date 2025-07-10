#include "utils.hpp"

#include "common/types.hpp"

namespace pldm::oem_meta
{

namespace
{

/* @brief map PLDM TID to MCTP EID; currently, they have a one-to-one
 *        correspondence
 */
eid mapTIDtoEID(const pldm_tid_t& tid)
{
    eid EID = static_cast<eid>(tid);
    return EID;
}

} // namespace

bool checkMetaIana(const pldm_tid_t& tid)
{
    [[maybe_unused]] const eid& EID = mapTIDtoEID(tid);
    return true;
}

std::string getSlotNumberStringByTID(const pldm_tid_t& tid)
{
    [[maybe_unused]] const eid& EID = mapTIDtoEID(tid);
    return std::string{"0"};
}

} // namespace pldm::oem_meta
