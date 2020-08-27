#pragma once

#include "common/utils.hpp"
#include "pldmd/handler.hpp"
#include "libpldmresponder/pdr_utils.hpp"
namespace pldm
{

namespace responder
{

namespace oem_platform
{

class Handler : public CmdHandler
{
  public:
    Handler(const pldm::utils::DBusHandler* dBusIntf) : dBusIntf(dBusIntf)
    {}

    virtual int getOemStateSensorReadingsHandler(
        /* uint16_t sensorId,*/ /*uint8_t sensorRearmCnt,*/ uint16_t entityType,
        uint16_t entityInstance, uint16_t stateSetId, uint8_t compSensorCnt,
        std::vector<get_sensor_state_field>& stateField) = 0;

    virtual int OemSetStateEffecterStatesHandler(
        /* uint16_t effecterId,*/ uint16_t entityType, uint16_t entityInstance,
        uint16_t stateSetId, uint8_t compEffecterCnt,
        const std::vector<set_effecter_state_field>& stateField) = 0;

    virtual void buildOEMPDR(pdr_utils::RepoInterface& repo) = 0;

    virtual ~Handler()
    {}

  protected:
    const pldm::utils::DBusHandler* dBusIntf;
};

} // namespace oem_platform

} // namespace responder

} // namespace pldm
