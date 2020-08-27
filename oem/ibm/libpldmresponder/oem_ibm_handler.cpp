#include "oem_ibm_handler.hpp"
#include "libpldmresponder/oem_handler.hpp"
#include "../../../libpldmresponder/pdr_utils.hpp"
#include "../../../libpldm/pdr.h"
#include "../../../libpldm/platform.h"
#include "libpldmresponder/platform.hpp"
#include "libpldmresponder/pdr.hpp"

namespace pldm
{

namespace responder
{

namespace oem_ibm_platform
{

int pldm::responder::oem_ibm_platform::Handler::
    getOemStateSensorReadingsHandler(
        uint16_t sensorId,
        /* uint8_t sensorRearmCnt,*/ uint16_t entityType, // sensorId and
                                                          // sensorRearmCnt not
                                                          // needed
        uint16_t entityInstance, uint16_t stateSetId, uint8_t compSensorCnt,
        std::vector<get_sensor_state_field>&
            stateField) // fill up the data before sending
{
    int rc = PLDM_SUCCESS;
    std::cout << "sensorId " << sensorId << " remove if not needed \n";
    std::cout << "compSensorCnt " << (uint32_t)compSensorCnt << "\n";
    std::cout << "stateField " << stateField.size() << "\n";

    stateField.clear();

    for (size_t i = 0; i < compSensorCnt; i++)
    {
        uint8_t sensorOpState{};
        // create a lambda
        if (entityType == 33 &&
            stateSetId == 32769) // create a new function for this in inband not
                                 // class member
        {
            sensorOpState = T_SIDE;
            if (entityInstance == 0)
            {
                auto currSide = codeUpdate.fetchCurrentBootSide();
                if (currSide == "P")
                {
                    sensorOpState = P_SIDE;
                }
            }
            else
            {
                auto nextSide = codeUpdate.fetchNextBootSide();
                if (nextSide == "P")
                {
                    sensorOpState = P_SIDE;
                }
            }
        }
        stateField.push_back({PLDM_SENSOR_ENABLED, PLDM_SENSOR_UNKNOWN,
                              PLDM_SENSOR_UNKNOWN, sensorOpState});
    }
    return rc;
}

int pldm::responder::oem_ibm_platform::Handler::
    OemSetStateEffecterStatesHandler(
        uint16_t effecterId, uint16_t entityType, uint16_t entityInstance,
        uint16_t stateSetId, uint8_t compEffecterCnt,
        const std::vector<set_effecter_state_field>& stateField)
{
    int rc = PLDM_SUCCESS;

    std::cout << "effecterId " << effecterId << "\n";
    std::cout << "entityType " << entityType << "\n";
    std::cout << "entityInstance " << entityInstance << "\n";
    std::cout << "stateSetId " << stateSetId << "\n";
    std::cout << "compEffecterCnt " << (uint32_t)compEffecterCnt << "\n";

    for (uint8_t currState = 0; currState < compEffecterCnt; ++currState)
    {
        if (stateField[currState].set_request == PLDM_REQUEST_SET)
        {

            if (entityType == 33 && stateSetId == 32769)
            {
                auto side = (stateField[currState].effecter_state == P_SIDE)
                                ? "P"
                                : "T";
                if (entityInstance == 0)
                {
                    rc = codeUpdate.setCurrentBootSide(side);
                }
                else
                {
                    rc = codeUpdate.setNextBootSide(side);
                }
            }
        }
        if (rc != PLDM_SUCCESS)
        {
            break;
        }
    }
    return rc;
}

void generateStateEffecterOEMPDR(pldm::responder::platform::Handler* handler,
                                uint16_t entityInstance, uint16_t stateSetID,
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
    pdr->effecter_id = handler->getNextEffecterId();
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
    pldm::responder::pdr_utils::PdrEntry pdrEntry{};
    pdrEntry.data = entry.data();
    pdrEntry.size = pdrSize;
    repo.addRecord(pdrEntry);
}

void generateStateSensorOEMPDR(pldm::responder::platform::Handler* handler,
                              uint16_t entityInstance, uint16_t stateSetID,
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
    pdr->sensor_id = handler->getNextSensorId();
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
    pldm::responder::pdr_utils::PdrEntry pdrEntry{};
    pdrEntry.data = entry.data();
    pdrEntry.size = pdrSize;
    repo.addRecord(pdrEntry);
}

void pldm::responder::oem_ibm_platform::Handler::buildOEMPDR(pdr_utils::RepoInterface& repo)
{
    generateStateEffecterOEMPDR(platformHandler,0, 32769, repo);

    generateStateEffecterOEMPDR(platformHandler,1, 32769, repo);

    generateStateEffecterOEMPDR(platformHandler,0, 32768, repo);

    generateStateSensorOEMPDR(platformHandler,0, 32769, repo);

    generateStateSensorOEMPDR(platformHandler,1, 32769, repo);

    generateStateSensorOEMPDR(platformHandler,0, 32768, repo);

}

void pldm::responder::oem_ibm_platform::Handler::setPlatformHandler(
    pldm::responder::platform::Handler* handler)
{
    platformHandler = handler;
    // example to call getNextEffecterId()
    auto effecterId = platformHandler->getNextEffecterId();
    std::cout << "generated effecter id " << effecterId << "\n";
}

} // namespace oem_ibm_platform

} // namespace responder

} // namespace pldm
