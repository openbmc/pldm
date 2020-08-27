#include "oem_ibm_handler.hpp"

#include "libpldm/entity.h"

#include "libpldmresponder/pdr_utils.hpp"

namespace pldm
{
namespace responder
{
namespace oem_ibm_platform
{

int pldm::responder::oem_ibm_platform::Handler::
    getOemStateSensorReadingsHandler(
        EntityType entityType, EntityInstance entityInstance,
        StateSetId stateSetId, CompositeCount compSensorCnt,
        std::vector<get_sensor_state_field>& stateField)
{
    int rc = PLDM_SUCCESS;
    stateField.clear();

    for (size_t i = 0; i < compSensorCnt; i++)
    {
        uint8_t sensorOpState{};
        if (entityType == PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE &&
            stateSetId == PLDM_OEM_IBM_BOOT_STATE)
        {
            sensorOpState = fetchBootSide(entityInstance, codeUpdate);
        }
        else
        {
            rc = PLDM_PLATFORM_INVALID_STATE_VALUE;
            break;
        }
        stateField.push_back({PLDM_SENSOR_ENABLED, PLDM_SENSOR_UNKNOWN,
                              PLDM_SENSOR_UNKNOWN, sensorOpState});
    }
    return rc;
}

int pldm::responder::oem_ibm_platform::Handler::
    oemSetStateEffecterStatesHandler(
        EntityType entityType, EntityInstance entityInstance,
        StateSetId stateSetId, CompositeCount compEffecterCnt,
        const std::vector<set_effecter_state_field>& stateField)
{
    int rc = PLDM_SUCCESS;

    for (uint8_t currState = 0; currState < compEffecterCnt; ++currState)
    {
        if (stateField[currState].set_request == PLDM_REQUEST_SET)
        {
            if (entityType == PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE &&
                stateSetId == PLDM_OEM_IBM_BOOT_STATE)
            {
                rc = setBootSide(entityInstance, currState, stateField,
                                 codeUpdate);
            }
            else
            {
                rc = PLDM_PLATFORM_SET_EFFECTER_UNSUPPORTED_SENSORSTATE;
            }
        }
        if (rc != PLDM_SUCCESS)
        {
            break;
        }
    }
    return rc;
}

void buildAllCodeUpdateEffecterPDR(platform::Handler* platformHandler,
                                   uint16_t entityInstance, uint16_t stateSetID,
                                   pdr_utils::Repo& repo)
{
    size_t pdrSize = 0;
    pdrSize = sizeof(pldm_state_effecter_pdr) +
              sizeof(state_effecter_possible_states);
    std::vector<uint8_t> entry{};
    entry.resize(pdrSize);
    pldm_state_effecter_pdr* pdr =
        reinterpret_cast<pldm_state_effecter_pdr*>(entry.data());
    if (!pdr)
    {
        std::cerr << "Failed to get record by PDR type, ERROR:"
                  << PLDM_PLATFORM_INVALID_EFFECTER_ID << std::endl;
    }
    pdr->hdr.record_handle = 0;
    pdr->hdr.version = 1;
    pdr->hdr.type = PLDM_STATE_EFFECTER_PDR;
    pdr->hdr.record_change_num = 0;
    pdr->hdr.length = sizeof(pldm_state_effecter_pdr) - sizeof(pldm_pdr_hdr);
    pdr->terminus_handle = pdr::BmcPldmTerminusHandle;
    pdr->effecter_id = platformHandler->getNextEffecterId();
    pdr->entity_type = PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE;
    pdr->entity_instance = entityInstance;
    pdr->container_id = 0;
    pdr->effecter_semantic_id = 0;
    pdr->effecter_init = PLDM_NO_INIT;
    pdr->has_description_pdr = false;
    pdr->composite_effecter_count = 1;

    auto* possibleStatesPtr = pdr->possible_states;
    auto possibleStates =
        reinterpret_cast<state_effecter_possible_states*>(possibleStatesPtr);
    possibleStates->state_set_id = stateSetID;
    possibleStates->possible_states_size = 2;
    auto state =
        reinterpret_cast<state_effecter_possible_states*>(possibleStates);
    if (stateSetID == PLDM_OEM_IBM_BOOT_STATE)
        state->states[0].byte = 6;
    else if (stateSetID == PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE)
        state->states[0].byte = 126;
    pldm::responder::pdr_utils::PdrEntry pdrEntry{};
    pdrEntry.data = entry.data();
    pdrEntry.size = pdrSize;
    repo.addRecord(pdrEntry);
}

void buildAllCodeUpdateSensorPDR(platform::Handler* platformHandler,
                                 uint16_t entityInstance, uint16_t stateSetID,
                                 pdr_utils::Repo& repo)
{
    size_t pdrSize = 0;
    pdrSize =
        sizeof(pldm_state_sensor_pdr) + sizeof(state_sensor_possible_states);
    std::vector<uint8_t> entry{};
    entry.resize(pdrSize);
    pldm_state_sensor_pdr* pdr =
        reinterpret_cast<pldm_state_sensor_pdr*>(entry.data());
    if (!pdr)
    {
        std::cerr << "Failed to get record by PDR type, ERROR:"
                  << PLDM_PLATFORM_INVALID_SENSOR_ID << std::endl;
    }
    pdr->hdr.record_handle = 0;
    pdr->hdr.version = 1;
    pdr->hdr.type = PLDM_STATE_SENSOR_PDR;
    pdr->hdr.record_change_num = 0;
    pdr->hdr.length = sizeof(pldm_state_sensor_pdr) - sizeof(pldm_pdr_hdr);
    pdr->terminus_handle = pdr::BmcPldmTerminusHandle;
    pdr->sensor_id = platformHandler->getNextSensorId();
    pdr->entity_type = PLDM_ENTITY_VIRTUAL_MACHINE_MANAGER;
    pdr->entity_instance = entityInstance;
    pdr->container_id = 0;
    pdr->sensor_init = PLDM_NO_INIT;
    pdr->sensor_auxiliary_names_pdr = false;
    pdr->composite_sensor_count = 1;

    auto* possibleStatesPtr = pdr->possible_states;
    auto possibleStates =
        reinterpret_cast<state_sensor_possible_states*>(possibleStatesPtr);
    possibleStates->state_set_id = stateSetID;
    possibleStates->possible_states_size = 2;
    auto state =
        reinterpret_cast<state_sensor_possible_states*>(possibleStates);
    if (stateSetID == PLDM_OEM_IBM_BOOT_STATE)
        state->states[0].byte = 6;
    else if (stateSetID == PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE)
        state->states[0].byte = 126;
    pldm::responder::pdr_utils::PdrEntry pdrEntry{};
    pdrEntry.data = entry.data();
    pdrEntry.size = pdrSize;
    repo.addRecord(pdrEntry);
}

void pldm::responder::oem_ibm_platform::Handler::buildOEMPDR(
    pdr_utils::Repo& repo)
{
    buildAllCodeUpdateEffecterPDR(platformHandler, ENTITY_INSTANCE_0,
                                  PLDM_OEM_IBM_BOOT_STATE, repo);
    buildAllCodeUpdateEffecterPDR(platformHandler, ENTITY_INSTANCE_1,
                                  PLDM_OEM_IBM_BOOT_STATE, repo);
    buildAllCodeUpdateEffecterPDR(platformHandler, ENTITY_INSTANCE_0,
                                  PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE, repo);

    buildAllCodeUpdateSensorPDR(platformHandler, ENTITY_INSTANCE_0,
                                PLDM_OEM_IBM_BOOT_STATE, repo);
    buildAllCodeUpdateSensorPDR(platformHandler, ENTITY_INSTANCE_1,
                                PLDM_OEM_IBM_BOOT_STATE, repo);
    buildAllCodeUpdateSensorPDR(platformHandler, ENTITY_INSTANCE_0,
                                PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE, repo);
}

void pldm::responder::oem_ibm_platform::Handler::setPlatformHandler(
    pldm::responder::platform::Handler* handler)
{
    platformHandler = handler;
}

} // namespace oem_ibm_platform

} // namespace responder

} // namespace pldm
