#include "oem_ibm_handler.hpp"

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

void pldm::responder::oem_ibm_platform::Handler::setPlatformHandler(
    pldm::responder::platform::Handler* handler)
{
    platformHandler = handler;
    // example to call getNextEffecterId()
    /*   auto effecterId = platformHandler->getNextEffecterId();
       std::cout << "generated effecter id " << effecterId << "\n";*/
}

} // namespace oem_ibm_platform

} // namespace responder

} // namespace pldm
