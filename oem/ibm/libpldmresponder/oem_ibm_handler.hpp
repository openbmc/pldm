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

#define PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE 32768
#define PLDM_OEM_IBM_BOOT_STATE 32769

class Handler : public oem_platform::Handler
{
  public:
    Handler(const pldm::utils::DBusHandler* dBusIntf,
            pldm::responder::CodeUpdate* codeUpdate) :
        oem_platform::Handler(dBusIntf),
        codeUpdate(codeUpdate), platformHandler(nullptr)
    {
        codeUpdate->setVersions();
    }

    int getOemStateSensorReadingsHandler(
        EntityType entityType, EntityInstance entityInstance,
        StateSetId stateSetId, CompositeCount compSensorCnt,
        std::vector<get_sensor_state_field>& stateField);

    int oemSetStateEffecterStatesHandler(
        EntityType entityType, EntityInstance entityInstance,
        StateSetId stateSetId, CompositeCount compEffecterCnt,
        const std::vector<set_effecter_state_field>& stateField);

    /** @brief Method to set the platform handler in the
     *         oem_ibm_handler class
     *  @param[in] handler - pointer to PLDM platform handler
     */
    void setPlatformHandler(pldm::responder::platform::Handler* handler);

    ~Handler() = default;

    pldm::responder::CodeUpdate* codeUpdate; //!< pointer to CodeUpdate object
    pldm::responder::platform::Handler*
        platformHandler; //!< pointer to PLDM platform handler
};

} // namespace oem_ibm_platform

} // namespace responder

} // namespace pldm
