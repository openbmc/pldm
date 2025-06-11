#include "oem/meta/event/oem_event_manager.hpp"
#include "oem/meta/libpldmresponder/file_io.hpp"
#include "oem/meta/requester/configuration_discovery_handler.hpp"

namespace pldm
{
namespace oem_meta
{

/**
 * @class OemMETA
 *
 * @brief class for creating all the OEM META handlers
 *
 *  Only in case of OEM_META this class object will be instantiated
 */

class OemMETA
{
  public:
    OemMETA() = delete;
    OemMETA& operator=(const OemMETA&) = delete;
    OemMETA(OemMETA&&) = delete;
    OemMETA& operator=(OemMETA&&) = delete;

  public:
    /** Constucts OemMETA object
     * @param[in] dBusIntf - D-Bus handler
     * @param[in] mctp_eid - MCTP EID of remote host firmware
     * @param[in] invoker - invoker handler
     * @param[in] platformHandler - platformHandler handler
     * @param[in] platformManager - MC Platform manager
     */

    explicit OemMETA(const pldm::utils::DBusHandler* dbusHandler,
                     uint8_t mctp_eid, pldm::responder::Invoker& invoker,
                     pldm::responder::platform::Handler* platformHandler) :
        mctp_eid(mctp_eid), invoker(invoker)
    {
        configurationDiscovery =
            std::make_shared<pldm::requester::ConfigurationDiscoveryHandler>(
                dbusHandler);

        fileIOHandler =
            std::make_unique<pldm::responder::oem_meta::FileIOHandler>(
                configurationDiscovery);
        registerHandler();

        oemEventManager =
            std::make_shared<oem_meta::OemEventManager>(configurationDiscovery);
        registerOemEventHandler(platformHandler);
    }

    pldm::requester::ConfigurationDiscoveryHandler*
        getMctpConfigurationHandler() const
    {
        return configurationDiscovery.get();
    }

  private:
    void registerHandler()
    {
        invoker.registerHandler(PLDM_OEM, std::move(fileIOHandler));
    }

    void registerOemEventHandler(
        pldm::responder::platform::Handler* platformHandler)
    {
        platformHandler->registerEventHandlers(
            PLDM_OEM_EVENT_CLASS_0xFB,
            {[this](const pldm_msg* request, size_t payloadLength,
                    uint8_t formatVersion, uint8_t tid,
                    size_t eventDataOffset) {
                return this->oemEventManager->handleOemEvent(
                    request, payloadLength, formatVersion, tid,
                    eventDataOffset);
            }});
    }

    /** @brief MCTP configurations handler*/
    std::shared_ptr<pldm::requester::ConfigurationDiscoveryHandler>
        configurationDiscovery{};

    std::unique_ptr<pldm::responder::oem_meta::FileIOHandler> fileIOHandler =
        nullptr;

    /** @brief MCTP EID of remote host firmware */
    uint8_t mctp_eid;

    /** @brief Object to the invoker class*/
    pldm::responder::Invoker& invoker;

    std::shared_ptr<oem_meta::OemEventManager> oemEventManager{};
};

} // namespace oem_meta
} // namespace pldm
