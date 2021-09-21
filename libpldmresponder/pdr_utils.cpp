#include "libpldm/fru.h"
#include "libpldm/platform.h"

#include "pdr.hpp"

#include <config.h>

#include <bitset>
#include <climits>

using namespace pldm::pdr;

namespace pldm
{

namespace responder
{

namespace pdr_utils
{

pldm_pdr* Repo::getPdr() const
{
    return repo;
}

RecordHandle Repo::addRecord(const PdrEntry& pdrEntry)
{
    return pldm_pdr_add(repo, pdrEntry.data, pdrEntry.size,
                        pdrEntry.handle.recordHandle, false, TERMINUS_HANDLE);
}

const pldm_pdr_record* Repo::getFirstRecord(PdrEntry& pdrEntry)
{
    constexpr uint32_t firstNum = 0;
    uint8_t* pdrData = nullptr;
    auto record =
        pldm_pdr_find_record(getPdr(), firstNum, &pdrData, &pdrEntry.size,
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

uint32_t Repo::getRecordHandle(const pldm_pdr_record* record) const
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

StatestoDbusVal populateMapping(const std::string& type, const Json& dBusValues,
                                const PossibleValues& pv)
{
    size_t pos = 0;
    pldm::utils::PropertyValue value;
    StatestoDbusVal valueMap;
    if (dBusValues.size() != pv.size())
    {
        std::cerr
            << "dBusValues size is not equal to pv size, dBusValues Size: "
            << dBusValues.size() << ", pv Size: " << pv.size() << "\n";

        return {};
    }

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
            return {};
        }

        valueMap.emplace(pv[pos], value);
    }

    return valueMap;
}

std::tuple<TerminusHandle, SensorID, SensorInfo>
    parseStateSensorPDR(const std::vector<uint8_t>& stateSensorPdr)
{
    auto pdr =
        reinterpret_cast<const pldm_state_sensor_pdr*>(stateSensorPdr.data());
    CompositeSensorStates sensors{};
    auto statesPtr = pdr->possible_states;
    auto compositeSensorCount = pdr->composite_sensor_count;
    std::vector<StateSetId> stateSetIds{};

    while (compositeSensorCount--)
    {
        auto state =
            reinterpret_cast<const state_sensor_possible_states*>(statesPtr);
        PossibleStates possibleStates{};
        uint8_t possibleStatesPos{};
        auto updateStates = [&possibleStates,
                             &possibleStatesPos](const bitfield8_t& val) {
            for (int i = 0; i < CHAR_BIT; i++)
            {
                if (val.byte & (1 << i))
                {
                    possibleStates.insert(possibleStatesPos * CHAR_BIT + i);
                }
            }
            possibleStatesPos++;
        };
        std::for_each(&state->states[0],
                      &state->states[state->possible_states_size],
                      updateStates);

        sensors.emplace_back(std::move(possibleStates));
        stateSetIds.emplace_back(state->state_set_id);

        if (compositeSensorCount)
        {
            statesPtr += sizeof(state_sensor_possible_states) +
                         state->possible_states_size - 1;
        }
    }

    auto entityInfo =
        std::make_tuple(static_cast<ContainerID>(pdr->container_id),
                        static_cast<EntityType>(pdr->entity_type),
                        static_cast<EntityInstance>(pdr->entity_instance));
    auto sensorInfo = std::make_tuple(std::move(entityInfo), std::move(sensors),
                                      std::move(stateSetIds));
    return std::make_tuple(pdr->terminus_handle, pdr->sensor_id,
                           std::move(sensorInfo));
}

std::vector<FruRecordDataFormat> parseFruRecordTable(const uint8_t* fruData,
                                                     size_t fruLen)
{
    // 7: uint16_t(FRU Record Set Identifier), uint8_t(FRU Record Type),
    // uint8_t(Number of FRU fields), uint8_t(Encoding Type for FRU fields),
    // uint8_t(FRU Field Type), uint8_t(FRU Field Length)
    if (fruLen < 7)
    {
        return {};
    }

    std::vector<FruRecordDataFormat> frus;

    size_t index = 0;
    do
    {
        FruRecordDataFormat fru;

        auto record = reinterpret_cast<const pldm_fru_record_data_format*>(
            fruData + index);
        fru.fruRSI = (int)le16toh(record->record_set_id);
        fru.fruRecType = record->record_type;
        fru.fruNum = record->num_fru_fields;
        fru.fruEncodeType = record->encoding_type;

        index += 5;

        for (int i = 0; i < record->num_fru_fields; i++)
        {
            auto tlv =
                reinterpret_cast<const pldm_fru_record_tlv*>(fruData + index);
            FruTLV frutlv;
            frutlv.fruFieldType = tlv->type;
            frutlv.fruFieldLen = tlv->length;
            frutlv.fruFieldValue.resize(tlv->length);
            for (int i = 0; i < tlv->length; i++)
            {
                memcpy(frutlv.fruFieldValue.data() + i, tlv->value + i, 1);
            }

            fru.fruTLV.push_back(frutlv);

            index += 2 + (unsigned)tlv->length;
        }

        frus.push_back(fru);

    } while (index < fruLen);

    return frus;
}

std::vector<uint8_t> fetchBitMap(const std::vector<std::vector<uint8_t>>& pdrs)
{
    std::vector<uint8_t> bitMap;
    for (const auto& pdr : pdrs)
    {
        auto effecterPdr =
            reinterpret_cast<const pldm_state_effecter_pdr*>(pdr.data());
        auto statesPtr = effecterPdr->possible_states;
        auto compEffCount = effecterPdr->composite_effecter_count;
        while (compEffCount--)
        {
            auto state =
                reinterpret_cast<const state_effecter_possible_states*>(
                    statesPtr);
            uint8_t possibleStatesPos{};
            auto printStates = [&possibleStatesPos,
                                &bitMap](const bitfield8_t& val) {
                bitMap.emplace_back(static_cast<uint8_t>(val.byte));
                possibleStatesPos++;
            };
            std::for_each(state->states,
                          state->states + state->possible_states_size,
                          printStates);
            if (compEffCount)
            {
                statesPtr += sizeof(state_effecter_possible_states) +
                             state->possible_states_size - 1;
            }
        }
    }
    return bitMap;
}

} // namespace pdr_utils
} // namespace responder
} // namespace pldm
