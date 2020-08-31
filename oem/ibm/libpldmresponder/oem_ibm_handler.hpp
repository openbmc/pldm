#pragma once

#include "inband_code_update.hpp"
#include "libpldmresponder/oem_handler.hpp"

namespace pldm
{

namespace responder
{

namespace oem_ibm_platform
{

class Handler : public oem_platform::Handler
{
  public:
    Handler(const pldm::utils::DBusHandler* dBusIntf) :
        oem_platform::Handler(dBusIntf), codeUpdate(dBusIntf)
    {}

    int getOemStateSensorReadingsHandler(
        uint16_t sensorId, uint8_t sensorRearmCnt, uint16_t entityType,
        uint16_t entityInstance, uint16_t stateSetId, uint8_t& compSensorCnt,
        std::vector<get_sensor_state_field>& stateField);

    int OemSetStateEffecterStatesHandler(uint16_t effecterId,
                                         uint16_t entityType,
                                         uint16_t entityInstance,
                                         uint16_t stateSetId,
                                         uint8_t compEffecterCnt);

    ~Handler()
    {}

  private:
    pldm::responder::CodeUpdate codeUpdate;
};

} // namespace oem_ibm_platform

} // namespace responder

} // namespace pldm
