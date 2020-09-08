#pragma once

#include "inband_code_update.hpp"
#include "libpldmresponder/oem_handler.hpp"
#include "libpldmresponder/platform.hpp"

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
        oem_platform::Handler(dBusIntf), codeUpdate(dBusIntf),
        platformHandler(nullptr)
    {}

    int getOemStateSensorReadingsHandler(
        uint16_t sensorId, /*uint8_t sensorRearmCnt,*/ uint16_t entityType,
        uint16_t entityInstance, uint16_t stateSetId, uint8_t compSensorCnt,
        std::vector<get_sensor_state_field>& stateField);

    int OemSetStateEffecterStatesHandler(
        uint16_t effecterId, uint16_t entityType, uint16_t entityInstance,
        uint16_t stateSetId, uint8_t compEffecterCnt,
        const std::vector<set_effecter_state_field>& stateField);
    void setPlatformHandler(pldm::responder::platform::Handler* handler);

    bool isCodeUpdateInProgress();

    std::string fetchCurrentBootSide();

    ~Handler()
    {}

  private:
    pldm::responder::CodeUpdate codeUpdate;
    pldm::responder::platform::Handler* platformHandler;
};

} // namespace oem_ibm_platform

} // namespace responder

} // namespace pldm
