#include "oem_ibm_handler.hpp"
#include "libpldmresponder/pdr_utils.hpp"

namespace pldm
{

namespace responder
{

namespace oem_ibm_platform
{

int pldm::responder::oem_ibm_platform::Handler::
    getOemStateSensorReadingsHandler(
        /*uint16_t sensorId,*/
        /* uint8_t sensorRearmCnt,*/ uint16_t entityType, // sensorId and
                                                          // sensorRearmCnt not
                                                          // needed
        uint16_t entityInstance, uint16_t stateSetId, uint8_t compSensorCnt,
        std::vector<get_sensor_state_field>&
            stateField) // fill up the data before sending
{
    int rc = PLDM_SUCCESS;
    std::cout << "getOemStateSensorReadingsHandler enter \n";

    stateField.clear();

    for (size_t i = 0; i < compSensorCnt; i++)
    {
        uint8_t sensorOpState{};
        if (entityType == 33 && stateSetId == 32769)
        {
            sensorOpState = fetchBootSide(entityInstance, codeUpdate);
            std::cout << "fetched sensorOpState " << (uint16_t)sensorOpState
                      << "\n";
        }
        else
        {
            rc = PLDM_PLATFORM_INVALID_STATE_VALUE;
            break;
        }
        stateField.push_back({PLDM_SENSOR_ENABLED, PLDM_SENSOR_UNKNOWN,
                              PLDM_SENSOR_UNKNOWN, sensorOpState});
        std::cout << "stateField[0].sensor_op_state "
                  << (uint16_t)stateField[0].sensor_op_state << "\n";
        std::cout << "stateField[0].present_state "
                  << (uint16_t)stateField[0].present_state << "\n";
        std::cout << "stateField[0].previous_state "
                  << (uint16_t)stateField[0].previous_state << "\n";
        std::cout << "stateField[0].event_state "
                  << (uint16_t)stateField[0].event_state << "\n";
    }
    std::cout << "getOemStateSensorReadingsHandler exit \n";
    return rc;
}

int pldm::responder::oem_ibm_platform::Handler::
    OemSetStateEffecterStatesHandler(
        /*uint16_t effecterId,*/ uint16_t entityType, uint16_t entityInstance,
        uint16_t stateSetId, uint8_t compEffecterCnt,
        const std::vector<set_effecter_state_field>& stateField)
{
    int rc = PLDM_SUCCESS;

    std::cout << "OemSetStateEffecterStatesHandler enter \n";

    for (uint8_t currState = 0; currState < compEffecterCnt; ++currState)
    {
        if (stateField[currState].set_request == PLDM_REQUEST_SET)
        {
            if (entityType == 33 && stateSetId == 32769)
            {
                std::cout << "calling setBootSide \n";
                rc = setBootSide(entityInstance, currState, stateField,
                                 codeUpdate);
                std::cout << "after setBootSide \n";
            }
            else
            {
                rc = PLDM_PLATFORM_SET_EFFECTER_UNSUPPORTED_SENSORSTATE;
                std::cout
                    << "returning "
                       "PLDM_PLATFORM_SET_EFFECTER_UNSUPPORTED_SENSORSTATE \n";
            }
        }
        if (rc != PLDM_SUCCESS)
        {
            break;
        }
    }
    return rc;
}

void pldm::responder::oem_ibm_platform::Handler::buildOEMPDR(
                                                 pdr_utils::RepoInterface& repo)
{
    buildAllCodeUpdateEffecterPDR(platformHandler,repo);
      
    buildAllCodeUpdateSensorPDR(platformHandler,repo);

}

void pldm::responder::oem_ibm_platform::Handler::setPlatformHandler(
    pldm::responder::platform::Handler* handler)
{
    platformHandler = handler;
    // example to call getNextEffecterId()
    /*   auto effecterId = platformHandler->getNextEffecterId();
       std::cout << "generated effecter id " << effecterId << "\n";*/
}

/*bool pldm::responder::oem_ibm_platform::Handler::isCodeUpdateInProgress()
{
    return codeUpdate.isCodeUpdateInProgress();
}

std::string pldm::responder::oem_ibm_platform::Handler::fetchCurrentBootSide()
{
    return codeUpdate.fetchCurrentBootSide();
}*/

} // namespace oem_ibm_platform

} // namespace responder

} // namespace pldm
