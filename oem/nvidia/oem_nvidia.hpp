#pragma once

#include "../../common/utils.hpp"
#include "../../libpldmresponder/base.hpp"
#include "../../libpldmresponder/bios.hpp"
#include "../../libpldmresponder/fru.hpp"
#include "../../libpldmresponder/platform.hpp"
#include "../../platform-mc/manager.hpp"
#include "../../pldmd/invoker.hpp"
#include "../../requester/request.hpp"

namespace pldm
{
namespace oem_nvidia
{

/**
 * @class OemNVIDIA
 *
 * @brief class for creating all the OEM NVIDIA handlers
 *
 *  Only in case of OEM_NVIDIA this class object will be instantiated
 */
class OemNVIDIA
{
  public:
    OemNVIDIA() = delete;
    OemNVIDIA& operator=(const OemNVIDIA&) = delete;
    OemNVIDIA(OemNVIDIA&&) = delete;
    OemNVIDIA& operator=(OemNVIDIA&&) = delete;

  public:
    /** Constructs OemNVIDIA object
     *
     * @param[in] dBusIntf - D-Bus handler
     * @param[in] mctp_fd - fd of MCTP communications socket
     * @param[in] mctp_eid - MCTP EID of remote host firmware
     * @param[in] repo - pointer to BMC's primary PDR repo
     * @param[in] instanceIdDb - pointer to an InstanceIdDb object
     * @param[in] event - sd_event handler
     * @param[in] invoker - invoker handler
     * @param[in] hostPDRHandler - hostPDRHandler handler
     * @param[in] platformHandler - platformHandler handler
     * @param[in] fruHandler - fruHandler handler
     * @param[in] baseHandler - baseHandler handler
     * @param[in] biosHandler - biosHandler handler
     * @param[in] reqHandler - reqHandler handler
     */
    explicit OemNVIDIA(
        const pldm::utils::DBusHandler* /* dBusIntf */, int /* mctp_fd */,
        pldm_pdr* /* repo */, pldm::InstanceIdDb& /* instanceIdDb */,
        sdeventplus::Event& /* event */, responder::Invoker& /* invoker */,
        HostPDRHandler* /* hostPDRHandler */,
        responder::platform::Handler* platformHandler,
        responder::fru::Handler* /* fruHandler */,
        responder::base::Handler* /* baseHandler */,
        responder::bios::Handler* /* biosHandler */,
        platform_mc::Manager* platformManager,
        pldm::requester::Handler<pldm::requester::Request>* /* reqHandler */) :
        platformHandler(platformHandler), platformManager(platformManager)
    {
        createOemEventHandler(this->platformHandler, this->platformManager);
    }

  private:
    /** @brief Method for creating OEM event handlers
     *
     *  This method registers handlers for backwards compatibility with
     *  event class 0xFA (legacy CPER event class before DSP0248 V1.3.0)
     */
    void createOemEventHandler(responder::platform::Handler* platformHandler,
                               platform_mc::Manager* platformManager)
    {
        /** CPEREvent class (0x07) is only available in DSP0248 V1.3.0.
         *  Before DSP0248 V1.3.0 spec, NVIDIA used OEM event class 0xFA to
         *  report CPER events. For backwards compatibility, handle both
         *  0xFA and standard PLDM_CPER_EVENT (0x07).
         */
        platformHandler->registerEventHandlers(
            0xFA,
            {[platformManager](const pldm_msg* request, size_t payloadLength,
                               uint8_t formatVersion, uint8_t tid,
                               size_t eventDataOffset) {
                return platformManager->handleCperEvent(
                    request, payloadLength, formatVersion, tid,
                    eventDataOffset);
            }});

        /* Support handling polled events with NVIDIA OEM CPER event class 0xFA
         */
        platformManager->registerPolledEventHandler(
            0xFA,
            {[platformManager](pldm_tid_t tid, uint16_t eventId,
                               const uint8_t* eventData, size_t eventDataSize) {
                return platformManager->handlePolledCperEvent(
                    tid, eventId, eventData, eventDataSize);
            }});
    }

  private:
    /** @brief Platform handler*/
    responder::platform::Handler* platformHandler = nullptr;

    /** @brief MC Platform manager*/
    platform_mc::Manager* platformManager = nullptr;
};

} // namespace oem_nvidia
} // namespace pldm
