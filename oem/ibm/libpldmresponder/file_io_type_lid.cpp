#include "file_io_type_lid.hpp"

#include "../../../libpldm/pdr.h"
#include "../../../libpldm/platform.h"

#include "../../../libpldmresponder/pdr_utils.hpp"
#include "file_io_by_type.hpp"
namespace pldm
{
namespace responder
{
// void generateStateEffecterPDRBootSide(
//                   pldm::responder::platform::Handler& handler,
//                 pldm::responder::pdr_utils::RepoInterface& repo)
void generateStateEffecterOEMPDR(uint16_t entityInstance, uint16_t stateSetID,
                                 pdr_utils::RepoInterface& repo)
{
    size_t pdrSize = 0;
    pdrSize = sizeof(pldm_state_effecter_pdr) +
              sizeof(state_effecter_possible_states);
    std::vector<uint8_t> entry{};
    entry.resize(pdrSize);
    pldm_state_effecter_pdr* pdr =
        reinterpret_cast<pldm_state_effecter_pdr*>(entry.data());

    pdr->hdr.record_handle = 0;
    pdr->hdr.version = 1;
    pdr->hdr.type = PLDM_STATE_EFFECTER_PDR;
    pdr->hdr.record_change_num = 0;
    pdr->hdr.length = sizeof(pldm_state_effecter_pdr) - sizeof(pldm_pdr_hdr);
    pdr->terminus_handle = pdr::BmcPldmTerminusHandle;
    // pdr->effecter_id = 0;
    pdr->entity_type = 33;
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
    if (stateSetID == 32769)
        state->states[0].byte = 6;
    else if (stateSetID == 32768)
        state->states[0].byte = 126;
    PdrEntry pdrEntry{};
    pdrEntry.data = entry.data();
    pdrEntry.size = pdrSize;
    repo.addRecord(pdrEntry);
}

void generateStateSensorOEMPDR(uint16_t entityInstance, uint16_t stateSetID,
                               pdr_utils::RepoInterface& repo)
{
    size_t pdrSize = 0;
    pdrSize =
        sizeof(pldm_state_sensor_pdr) + sizeof(state_sensor_possible_states);
    std::vector<uint8_t> entry{};
    entry.resize(pdrSize);
    pldm_state_sensor_pdr* pdr =
        reinterpret_cast<pldm_state_sensor_pdr*>(entry.data());

    pdr->hdr.record_handle = 0;
    pdr->hdr.version = 1;
    pdr->hdr.type = PLDM_STATE_SENSOR_PDR;
    pdr->hdr.record_change_num = 0;
    pdr->hdr.length = sizeof(pldm_state_sensor_pdr) - sizeof(pldm_pdr_hdr);
    pdr->terminus_handle = pdr::BmcPldmTerminusHandle;
    pdr->entity_type = 33;
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
    if (stateSetID == 32769)
        state->states[0].byte = 6;
    else if (stateSetID == 32768)
        state->states[0].byte = 126;
    PdrEntry pdrEntry{};
    pdrEntry.data = entry.data();
    pdrEntry.size = pdrSize;
    repo.addRecord(pdrEntry);
}

// int buildIbmOEMPDR(pldm::responder::platform::Handler& handler,
//                 pldm::responder::pdr_utils::RepoInterface& repo)
void buildIbmOEMPDR(pdr_utils::RepoInterface& repo)
{
    generateStateEffecterOEMPDR(0, 32769, repo);

    generateStateEffecterOEMPDR(1, 32769, repo);

    generateStateEffecterOEMPDR(0, 32768, repo);

    generateStateSensorOEMPDR(0, 32769, repo);

    generateStateSensorOEMPDR(1, 32769, repo);

    generateStateSensorOEMPDR(0, 32768, repo);
}

} // namespace responder
} // namespace pldm
