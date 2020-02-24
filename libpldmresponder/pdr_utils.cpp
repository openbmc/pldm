#include "pdr.hpp"

namespace pldm
{

namespace responder
{

namespace pdr_utils
{

const pldm_pdr* Repo::getPdr()
{
    return repo;
}

RecordHandle Repo::addRecord(const PdrEntry& pdrEntry)
{
    return pldm_pdr_add(repo, pdrEntry.data, pdrEntry.size,
                        pdrEntry.handle.recordHandle);
}

const pldm_pdr_record* Repo::getFirstRecord(PdrEntry& pdrEntry)
{
    constexpr uint32_t fistNum = 0;
    uint8_t* pdrData = nullptr;
    auto record =
        pldm_pdr_find_record(getPdr(), fistNum, &pdrData, &pdrEntry.size,
                             &pdrEntry.handle.nextRecordHandle);
    if (record)
    {
        pdrEntry.data = pdrData;
    }

    return record;
}

const pldm_pdr_record* Repo::getNextRecord(const pldm_pdr_record* currRecord,
                                           PdrEntry& pdrEntry)
{
    uint8_t* pdrData = nullptr;
    auto record =
        pldm_pdr_get_next_record(getPdr(), currRecord, &pdrData, &pdrEntry.size,
                                 &pdrEntry.handle.nextRecordHandle);
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

} // namespace pdr_utils
} // namespace responder
} // namespace pldm
