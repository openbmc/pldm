#pragma once
#include "../../common/utils.hpp"
#include "../../libpldmresponder/base.hpp"
#include "../../libpldmresponder/platform.hpp"
#include "../../oem/meta/event/oem_event_manager.hpp"
#include "../../platform-mc/manager.hpp"
#include "../../pldmd/invoker.hpp"
#include "../../requester/request.hpp"

namespace pldm
{
namespace oem_meta
{

/**
 * @class OemMeta
 *
 * @brief class for creating all the OEM META handlers
 *
 *  Only in case of OEM_META this class object will be instantiated
 */
class OemMeta
{
  public:
    OemMeta() = delete;
    OemMeta& operator=(const OemMeta&) = delete;
    OemMeta(OemMeta&&) = delete;
    OemMeta& operator=(OemMeta&&) = delete;

  public:
    /** Constructs OemMeta object
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
     * @param[in] baseHandler - baseHandler handler
     * @param[in] reqHandler - reqHandler handler
     */
    explicit OemMeta(
        const pldm::utils::DBusHandler* /* dBusIntf */, int /* mctp_fd */,
        pldm_pdr* /* repo */, pldm::InstanceIdDb& instanceIdDb,
        sdeventplus::Event& event, responder::Invoker& /* invoker */,
        HostPDRHandler* /* hostPDRHandler */,
        responder::platform::Handler* platformHandler,
        responder::base::Handler* /* baseHandler */,
        platform_mc::Manager* platformManager,
        pldm::requester::Handler<pldm::requester::Request>* reqHandler) :
        instanceIdDb(instanceIdDb), event(event),
        platformHandler(platformHandler), platformManager(platformManager),
        reqHandler(reqHandler)
    {
        oemEventManager = std::make_shared<oem_meta::OemEventManager>(
            this->event, this->reqHandler, this->instanceIdDb,
            this->platformManager);
        createOemEventHandler(oemEventManager.get(), this->platformManager);
    }

  private:
    /** @brief Method for creating OemEventManager
     *
     *  This method also assigns the OemEventManager to the below
     *  different handlers.
     */
    void createOemEventHandler(oem_meta::OemEventManager* oemEventManager,
                               platform_mc::Manager* platformManager)
    {
        /* Register Meta OEM handler to the PLDM CPER events */
        platformManager->registerPolledEventHandler(
            0xFB,
            {[oemEventManager](pldm_tid_t tid, uint16_t eventId,
                               const uint8_t* eventData, size_t eventDataSize) {
                return oemEventManager->processOemMsgPollEvent(
                    tid, eventId, eventData, eventDataSize);
            }});
    }

  private:
    /** @brief reference to an Instance ID database object, used to obtain PLDM
     * instance IDs
     */
    pldm::InstanceIdDb& instanceIdDb;

    /** @brief reference of main event loop of pldmd, primarily used to schedule
     *  work
     */
    sdeventplus::Event& event;

    /** @brief Platform handler*/
    responder::platform::Handler* platformHandler = nullptr;

    /** @brief MC Platform manager*/
    platform_mc::Manager* platformManager = nullptr;

    /** @brief pointer to the requester class*/
    requester::Handler<requester::Request>* reqHandler = nullptr;

    std::shared_ptr<oem_meta::OemEventManager> oemEventManager{};
};

} // namespace oem_meta
} // namespace pldm
