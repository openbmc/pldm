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

static constexpr auto PLDM_OEM_IBM_BOOT_STATE = 32769;
static constexpr auto PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE = 32768;
constexpr uint16_t ENTITY_INSTANCE_0 = 0;
constexpr uint16_t ENTITY_INSTANCE_1 = 1;
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
        uint16_t entityType, uint16_t entityInstance, uint16_t stateSetId,
        uint8_t compSensorCnt, std::vector<get_sensor_state_field>& stateField);

    int OemSetStateEffecterStatesHandler(
        uint16_t entityType, uint16_t entityInstance, uint16_t stateSetId,
        uint8_t compEffecterCnt,
        const std::vector<set_effecter_state_field>& stateField);

    /** @brief Method to set the platform handler in the
     *         oem_ibm_handler class
     *  @param[in] handler - pointer to PLDM platform handler
     */
    void setPlatformHandler(pldm::responder::platform::Handler* handler);

    /** @brief Method to Generate the OEM PDRs if OEM_IBM is defined
     *
     * @param[in] repo - instance of concrete implementation of Repo
     */
    void buildOEMPDR(pdr_utils::RepoInterface& repo);

    ~Handler()
    {}

    pldm::responder::CodeUpdate* codeUpdate; //!< pointer to CodeUpdate object
    pldm::responder::platform::Handler*
        platformHandler; //!< pointer to PLDM platform handler
};

} // namespace oem_ibm_platform

} // namespace responder

} // namespace pldm
