#include "file_io_by_type.hpp"

#include "file_io_type_lid.hpp"
#include "../../../libpldmresponder/pdr_utils.hpp"
#include "../../../libpldm/platform.h"
namespace pldm
{
namespace responder
{
void generateStateEffecterPDRBootSide(
                     pldm::responder::platform::Handler& handler, 
                     pldm::responder::pdr_utils::RepoInterface& repo)
{
    std::vector<uint8_t> pdrBuffer(sizeof(pldm_state_effecter_pdr));    
    auto pdr = reinterpret_cast<pldm_state_effecter_pdr*>(pdrBuffer.data()); 
    
    pdr->hdr.record_handle = 0;
    pdr->hdr.version = 1;
    pdr->hdr.type = PLDM_STATE_EFFECTER_PDR;
    pdr->hdr.record_change_num = 0;
    pdr->hdr.length = sizeof(pldm_state_effecter_pdr) - sizeof(pldm_pdr_hdr);
    pdr->terminus_handle = pdr::BmcPldmTerminusHandle;
    pdr->effecter_id = handler.getNextEffecterId();
    pdr->entity_type = 33;
    pdr->entity_instance = 0;
    pdr->container_id = 0;
    pdr->effecter_semantic_id = 0;
    pdr->effecter_init = PLDM_NO_INIT;
    pdr->has_description_pdr = false;
    pdr->composite_effecter_count = 0;
    
    state_effecter_possible_states* possibleStates = 
        (state_effecter_possible_states*)malloc(sizeof(
        state_effecter_possible_states)); 
    possibleStates->state_set_id = 32769;
    possibleStates->possible_states_size = 1;
    
    PossibleValues states;
    states.assign(1,2);
    for (const auto& state : states)
    {
        auto index = state / 8;
        auto bit = state - (index * 8);
        bitfield8_t* bf = (bitfield8_t*)malloc(sizeof(bitfield8_t));
        bf->byte |= 1 << bit;
    }
   
    PdrEntry pdrEntry{};
    pdrEntry.data = pdrBuffer.data();
    pdrEntry.size = pdrBuffer.size();
    repo.addRecord(pdrEntry);

    
}


void buildIbmOEMPDR(pldm::responder::platform::Handler& handler, 
                   pldm::responder::pdr_utils::RepoInterface& repo)
{
    generateStateEffecterPDRBootSide(handler, repo);

    //generateStateSensorPDRBootSide(repo);

    //generateStateSensorPDRFWUpdate(repo);

    //generateStateEffectorPDRFWUpdate(repo);

}

} // namespace responder
} // namespace pldm
