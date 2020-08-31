#include "oem_ibm_handler.hpp"

namespace pldm
{

namespace responder
{

namespace oem_ibm_platform
{

int pldm::responder::oem_ibm_platform::Handler::
    getOemStateSensorReadingsHandler(
        uint16_t sensorId, uint8_t sensorRearmCnt, uint16_t entityType,
        uint16_t entityInstance, uint16_t stateSetId, uint8_t& compSensorCnt,
        std::vector<get_sensor_state_field>&
            stateField) // fill up the data before sending
{
    int rc = PLDM_SUCCESS;
    std::cout << "sensorId " << sensorId << " remove if not needed \n";
    std::cout << "sensorRearmCnt " << (uint32_t)sensorRearmCnt << "\n";
    std::cout << "compSensorCnt " << (uint32_t)compSensorCnt << "\n";
    std::cout << "stateField " << stateField.size() << "\n";

    if (entityType == 33 && stateSetId == 32769)
    {
        if (entityInstance == 0)
        {
            auto currSide = codeUpdate.fetchCurrentBootSide();
        }
        else
        {
            auto nextSide = codeUpdate.fetchNextBootSide();
        }
    }
    return rc;
}

int pldm::responder::oem_ibm_platform::Handler::
    OemSetStateEffecterStatesHandler(uint16_t effecterId, uint16_t entityType,
                                     uint16_t entityInstance,
                                     uint16_t stateSetId,
                                     uint8_t compEffecterCnt)
{
    int rc = PLDM_SUCCESS;

    std::cout << "effecterId " << effecterId << "\n";
    std::cout << "entityType " << entityType << "\n";
    std::cout << "entityInstance " << entityInstance << "\n";
    std::cout << "stateSetId " << stateSetId << "\n";
    std::cout << "compEffecterCnt " << (uint32_t)compEffecterCnt << "\n";

    if (entityType == 33 && stateSetId == 32769)
    {
        std::string side("P");
        if (entityInstance == 0)
        {
            rc = codeUpdate.setCurrentBootSide(
                side); // change based on state field
        }
        else
        {
            rc = codeUpdate.setNextBootSide(side);
        }
    }
    return rc;
}

} // namespace oem_ibm_platform

} // namespace responder

} // namespace pldm
