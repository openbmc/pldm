#include "libpldm/platform.h"

#include "pdr.hpp"

#include <climits>

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
                        pdrEntry.handle.recordHandle, false);
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
    auto sensorInfo =
        std::make_tuple(std::move(entityInfo), std::move(sensors));
    return std::make_tuple(pdr->terminus_handle, pdr->sensor_id,
                           std::move(sensorInfo));
}

void normalizeHostPDR(pldm_pdr* repo, pldm_entity_association_tree* entityTree,
                      const std::vector<uint8_t>& pdrTypes)
{
    uint8_t* pdrData = nullptr;
    uint32_t pdrSize{};
    uint32_t nextRecHdl{};

    auto record =
        pldm_pdr_find_record(repo, 0, &pdrData, &pdrSize, &nextRecHdl);

    if (!record)
    {
        std::cerr << "no pdr in the repo to normalize \n";
        return;
    }
    do
    {
        auto pdrHdr = reinterpret_cast<pldm_pdr_hdr*>(pdrData);
        auto pdrType = pdrHdr->type;
        if (pldm_pdr_record_is_remote(record) &&
            std::find(pdrTypes.begin(), pdrTypes.end(), pdrType) !=
                pdrTypes.end())
        {
            pldm_entity entity{};
            switch (pdrType)
            {
                case PLDM_NUMERIC_SENSOR_PDR:
                    break;
                case PLDM_STATE_SENSOR_PDR:
                {
                    auto pdr =
                        reinterpret_cast<pldm_state_sensor_pdr*>(pdrData);
                    entity.entity_type = pdr->entity_type;
                    entity.entity_instance_num = pdr->entity_instance;
                    if (updateContainerId(entity, entityTree))
                    {
                        pdr->container_id = entity.entity_container_id;
                    }
                }
                break;
                case PLDM_NUMERIC_EFFECTER_PDR:
                {
                    auto pdr =
                        reinterpret_cast<pldm_numeric_effecter_value_pdr*>(
                            pdrData);
                    entity.entity_type = pdr->entity_type;
                    entity.entity_instance_num = pdr->entity_instance;
                    if (updateContainerId(entity, entityTree))
                    {
                        pdr->container_id = entity.entity_container_id;
                    }
                }
                break;
                case PLDM_STATE_EFFECTER_PDR:
                {
                    auto pdr =
                        reinterpret_cast<pldm_state_effecter_pdr*>(pdrData);
                    entity.entity_type = pdr->entity_type;
                    entity.entity_instance_num = pdr->entity_instance;
                    if (updateContainerId(entity, entityTree))
                    {
                        pdr->container_id = entity.entity_container_id;
                    }
                }
                break;
                case PLDM_PDR_FRU_RECORD_SET:
                {
                    auto pdr =
                        reinterpret_cast<pldm_pdr_fru_record_set*>(pdrData);
                    entity.entity_type = pdr->entity_type;
                    entity.entity_instance_num = pdr->entity_instance_num;
                    if (updateContainerId(entity, entityTree))
                    {
                        pdr->container_id = entity.entity_container_id;
                    }
                }
                break;
                default:
                    break;
            }
        }
        pdrData = nullptr;
        record = pldm_pdr_get_next_record(repo, record, &pdrData, &pdrSize,
                                          &nextRecHdl);
    } while (record);
}

bool updateContainerId(pldm_entity& entity,
                       pldm_entity_association_tree* entityTree)
{
    auto node = pldm_entity_association_tree_find(entityTree, &entity);

    if (node)
    {
        return true;
    }
    return false;
}

} // namespace pdr_utils
} // namespace responder
} // namespace pldm
