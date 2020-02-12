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

const pldm_pdr_record* Repo::getNextRecord(const pldm_pdr_record* curr_record,
                                           PdrEntry& pdrEntry)
{
    uint8_t* pdrData = nullptr;
    auto record = pldm_pdr_get_next_record(getPdr(), curr_record, &pdrData,
                                           &pdrEntry.size,
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

DbusIdToValMap populateMapping(const std::string& type, const Json& dBusValues,
                               const PossibleValues& pv)
{
    size_t pos = 0;
    PropertyValue value;
    DbusIdToValMap valueMap;
    for (auto it = dBusValues.begin(); it != dBusValues.end(); ++it, ++pos)
    {
        if (type == "uint8_t")
        {
            value = static_cast<uint8_t>(it.value());
        }
        else if (type == "uint16_t")
        {
            value = static_cast<uint16_t>(it.value());
        }
        else if (type == "uint32_t")
        {
            value = static_cast<uint32_t>(it.value());
        }
        else if (type == "uint64_t")
        {
            value = static_cast<uint64_t>(it.value());
        }
        else if (type == "int16_t")
        {
            value = static_cast<int16_t>(it.value());
        }
        else if (type == "int32_t")
        {
            value = static_cast<int32_t>(it.value());
        }
        else if (type == "int64_t")
        {
            value = static_cast<int64_t>(it.value());
        }
        else if (type == "bool")
        {
            value = static_cast<bool>(it.value());
        }
        else if (type == "double")
        {
            value = static_cast<double>(it.value());
        }
        else if (type == "string")
        {
            value = static_cast<std::string>(it.value());
        }
        else
        {
            std::cerr << "Unknown D-Bus property type, TYPE=" << type.c_str()
                      << "\n";
        }

        valueMap.emplace(pv[pos], value);
    }

    return valueMap;
}

} // namespace pdr_utils
} // namespace responder
} // namespace pldm
