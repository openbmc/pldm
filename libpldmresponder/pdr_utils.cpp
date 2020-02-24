#include "pdr.hpp"

namespace pldm
{

namespace responder
{

namespace pdrUtils
{

const pldm_pdr* Repo::getPdr()
{
    return this->repo;
}

RecordHandle Repo::addRecord(PdrEntry pdrEntry)
{
    return pldm_pdr_add(this->repo, pdrEntry.data, pdrEntry.size,
                        pdrEntry.handle.current_handle);
}

const pldm_pdr_record* Repo::getFirstRecord(PdrEntry& pdrEntry)
{
    uint8_t* pdrData = nullptr;
    auto record = pldm_pdr_find_record(getPdr(), 0, &pdrData, &pdrEntry.size,
                                       &pdrEntry.handle.next_handle);
    if (record)
    {
        pdrEntry.data = pdrData;
    }

    return record;
}

const pldm_pdr_record* Repo::getNextRecord(const pldm_pdr_record* curr_record,
                                           PdrEntry& pdrEntry)
{
    uint8_t* pdrData = nullptr;
    auto record =
        pldm_pdr_get_next_record(getPdr(), curr_record, &pdrData,
                                 &pdrEntry.size, &pdrEntry.handle.next_handle);
    if (record)
    {
        pdrEntry.data = pdrData;
    }

    return record;
}

uint32_t Repo::getRecordHandle(const pldm_pdr_record* record)
{
    return pldm_pdr_get_record_handle(getPdr(), record);
}

uint32_t Repo::getRecordCount()
{
    return pldm_pdr_get_record_count(getPdr());
}

bool Repo::empty()
{
    return !getRecordCount();
}

} // namespace pdrUtils
} // namespace responder
} // namespace pldm
