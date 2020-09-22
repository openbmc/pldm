#include "oem_ibm_handler.hpp"

#include "libpldm/requester/pldm.h"

#include "libpldmresponder/file_io.hpp"
#include "libpldmresponder/pdr_utils.hpp"

namespace pldm
{

namespace responder
{

namespace oem_ibm_platform
{

int pldm::responder::oem_ibm_platform::Handler::
    getOemStateSensorReadingsHandler(
        uint16_t entityType, uint16_t entityInstance, uint16_t stateSetId,
        uint8_t compSensorCnt, std::vector<get_sensor_state_field>& stateField)
{
    int rc = PLDM_SUCCESS;
    stateField.clear();

    for (size_t i = 0; i < compSensorCnt; i++)
    {
        uint8_t sensorOpState{};
        if (entityType == PLDM_VIRTUAL_MACHINE_MANAGER_ENTITY &&
            stateSetId == PLDM_OEM_IBM_BOOT_STATE)
        {
            sensorOpState = fetchBootSide(entityInstance, codeUpdate);
        }
        else
        {
            rc = PLDM_PLATFORM_INVALID_STATE_VALUE;
            break;
        }
        stateField.push_back({PLDM_SENSOR_ENABLED, PLDM_SENSOR_UNKNOWN,
                              PLDM_SENSOR_UNKNOWN, sensorOpState});
    }
    return rc;
}

int pldm::responder::oem_ibm_platform::Handler::
    OemSetStateEffecterStatesHandler(
        uint16_t entityType, uint16_t entityInstance, uint16_t stateSetId,
        uint8_t compEffecterCnt,
        const std::vector<set_effecter_state_field>& stateField)
{
    int rc = PLDM_SUCCESS;

    for (uint8_t currState = 0; currState < compEffecterCnt; ++currState)
    {
        if (stateField[currState].set_request == PLDM_REQUEST_SET)
        {
            if (entityType == PLDM_VIRTUAL_MACHINE_MANAGER_ENTITY &&
                stateSetId == PLDM_OEM_IBM_BOOT_STATE)
            {
                rc = setBootSide(entityInstance, currState, stateField,
                                 codeUpdate);
            }
            else if (entityType == 33 && stateSetId == 32768)
            {
                if (stateField[currState].effecter_state == START)
                {
                    codeUpdate->setCodeUpdateProgress(true);
                    rc = codeUpdate->setRequestedApplyTime();
                }
                else if (stateField[currState].effecter_state == END)
                {
                    codeUpdate->setCodeUpdateProgress(false);
                    // int  retc = assembleImage(LID_STAGING_DIR);
                    // if (retc == PLDM_SUCCESS)
                    rc = codeUpdate->setRequestedActivation(codeUpdate);
                    // else
                    // std::cerr << "Image assembly Failed ERROR:" << retc
                    //        << "\n";
                }
                else if (stateField[currState].effecter_state == ABORT)
                {
                    codeUpdate->setCodeUpdateProgress(false);
                    std::unique_ptr<oem_platform::Handler> oemPlatformHandler{};
                    oem_ibm::Handler handler(oemPlatformHandler.get());
                    rc = handler.clearDirPath(LID_STAGING_DIR);
                    std::cout << "Property Set" << std::endl;
                    // rc = codeUpdate->clearLids(platformHandler);
                }
                else if (stateField[currState].effecter_state == ACCEPT)
                {
                    // TODO Set new Dbus property provided by code update app
                }
                else if (stateField[currState].effecter_state == REJECT)
                {
                    // TODO Set new Dbus property provided by code update app
                }
            }
            else
            {
                rc = PLDM_PLATFORM_SET_EFFECTER_UNSUPPORTED_SENSORSTATE;
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
    buildAllCodeUpdateEffecterPDR(platformHandler, repo);

    buildAllCodeUpdateSensorPDR(platformHandler, repo);
}

void pldm::responder::oem_ibm_platform::Handler::setPlatformHandler(
    pldm::responder::platform::Handler* handler)
{
    platformHandler = handler;
}

int pldm::responder::oem_ibm_platform::Handler::sendEventToHost(
    std::vector<uint8_t>& requestMsg)
{
    uint8_t* responseMsg = nullptr;
    size_t responseMsgSize{};

    auto requesterRc =
        pldm_send_recv(mctp_eid, mctp_fd, requestMsg.data(), requestMsg.size(),
                       &responseMsg, &responseMsgSize);
    std::unique_ptr<uint8_t, decltype(std::free)*> responseMsgPtr{responseMsg,
                                                                  std::free};
    if (requesterRc != PLDM_REQUESTER_SUCCESS)
    {
        return requesterRc;
    }
    uint8_t completionCode{};
    uint8_t status{};
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsgPtr.get());
    auto rc = decode_platform_event_message_resp(
        responsePtr, responseMsgSize - sizeof(pldm_msg_hdr), &completionCode,
        &status);

    if (completionCode != PLDM_SUCCESS)
    {
        return PLDM_ERROR;
    }

    return rc;
}

void pldm::responder::oem_ibm_platform::Handler::sendCodeUpdateEvent(
    uint16_t effecterId, std::vector<set_effecter_state_field>& stateField,
    uint8_t compEffCnt)
{
    std::vector<uint8_t> requestMsg(
        sizeof(pldm_msg_hdr) + sizeof(effecterId) + sizeof(compEffCnt) +
            sizeof(set_effecter_state_field) * compEffCnt,
        0);

    auto instanceId = requester.getInstanceId(mctp_eid);

    auto rc = encodeEventMsg(effecterId, compEffCnt, stateField, requestMsg,
                             instanceId);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Failed to encode state sensor event, rc = " << rc
                  << std::endl;
        return;
    }

    rc = sendEventToHost(requestMsg);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Failed to send event to host: "
                  << "rc=" << rc << std::endl;
    }

    requester.markFree(mctp_eid, instanceId);

    return;
}

int encodeEventMsg(uint16_t effecterId, uint8_t compEffCnt,
                   std::vector<set_effecter_state_field>& stateField,
                   std::vector<uint8_t>& requestMsg, uint8_t instanceId)
{
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = encode_set_state_effecter_states_req(
        instanceId, effecterId, compEffCnt, stateField.data(), request);

    return rc;
}

} // namespace oem_ibm_platform

} // namespace responder

} // namespace pldm
