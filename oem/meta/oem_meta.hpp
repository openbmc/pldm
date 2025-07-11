#include "libpldmresponder/platform.hpp"
#include "oem/meta/event/oem_event_manager.hpp"
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
     * @param[in] invoker - invoker handler
     * @param[in] platformHandler - platformHandler handler
     * @param[in] platformManager - MC Platform manager
     */

    explicit OemMETA(const pldm::utils::DBusHandler* dbusHandler,
                     pldm::responder::platform::Handler* platformHandler)
    {
        configurationDiscovery = std::make_shared<
            pldm::requester::oem_meta::ConfigurationDiscoveryHandler>(
            dbusHandler);

        oemEventManager =
            std::make_shared<oem_meta::OemEventManager>(configurationDiscovery);
        registerOemEventHandler(platformHandler);
    }

    pldm::requester::oem_meta::ConfigurationDiscoveryHandler*
        getMctpConfigurationHandler() const
    {
        return configurationDiscovery.get();
    }

  private:
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
    std::shared_ptr<pldm::requester::oem_meta::ConfigurationDiscoveryHandler>
        configurationDiscovery{};

    std::shared_ptr<oem_meta::OemEventManager> oemEventManager{};
};

} // namespace oem_meta
} // namespace pldm
