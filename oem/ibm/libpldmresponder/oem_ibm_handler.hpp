#pragma once

#include "inband_code_update.hpp"
#include "libpldmresponder/oem_handler.hpp"
#include "libpldmresponder/platform.hpp"
#include "libpldmresponder/pdr_utils.hpp"
#include "libpldm/platform.h"

#include <memory>
#include <vector>
namespace pldm
{

namespace responder
{

namespace oem_ibm_platform
{

static constexpr auto PLDM_OEM_IBM_BOOT_STATE = 32769;
static constexpr auto PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE = 32768;

using SensorId = uint16_t;

class Handler : public oem_platform::Handler
{
  public:
    Handler(const pldm::utils::DBusHandler* dBusIntf,
            pldm::responder::CodeUpdate* codeUpdate, int mctp_fd, uint8_t mctp_eid, Requester& requester) :
        oem_platform::Handler(dBusIntf), 
        codeUpdate(codeUpdate), platformHandler(nullptr), mctp_fd(mctp_fd), mctp_eid(mctp_eid), requester(requester)
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

    uint16_t getNextEffecterId()
    {
       return platformHandler->getNextEffecterId();
    }

    uint16_t getNextSensorId()
    {
       return platformHandler->getNextSensorId();
    }

    void buildOEMPDR(pdr_utils::RepoInterface& repo);

    /** @brief Method to encode code update event msg
     *  @param[in] eventType - type of event
     *  @param[in] eventDataVec - vector of event data to be sent to host
     *  @param[in] instanceId - instance ID
     *  @param[in/out] requestMsg - request msg to be encoded
     *  @return PLDM status code
     */
    int encodeEventMsg(uint8_t eventType, const std::vector<uint8_t>& eventDataVec,
                       std::vector<uint8_t>& requestMsg, uint8_t instanceId);

    /** @brief Method to send code update event to host
     *  @param[in] sensorId - sendor ID
     *  @param[in] eventState - new code update event state
     *  @param[in] previousEventState - previous code update event state
     *  @param[in] stateSenserEventPtr - offset of state sensor
     *  @return void
     */
    void sendCodeUpdateEvent(SensorId sensorId, uint8_t eventState, uint8_t previousEventState, uint8_t stateSenserEventPtr);

    /** @brief Method to send encoded request msg of code update event to host
     *  @param[in] requestMsg - encoded request msg
     *  @return PLDM status code
     */
    int sendEventToHost(std::vector<uint8_t>& requestMsg);

    ~Handler()
    {}

    pldm::responder::CodeUpdate* codeUpdate; //!< pointer to CodeUpdate object
    pldm::responder::platform::Handler*
        platformHandler; //!< pointer to PLDM platform handler

    /** @brief fd of MCTP communications socket */
    int mctp_fd;

    /** @brief MCTP EID of host firmware */
    uint8_t mctp_eid;

    /** @brief reference to Requester object, primarily used to access API to
     *  obtain PLDM instance id.
     */
    Requester& requester;
};

} // namespace oem_ibm_platform

} // namespace responder

} // namespace pldm
