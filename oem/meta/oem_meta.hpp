#include "oem/meta/libpldmresponder/oem_meta_file_io.hpp"
#include "oem/meta/platform-mc/event_oem_meta_hpp"
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
    OemMETA(const Pdr&) = delete;
    OemMETA& operator=(const OemMETA&) = delete;
    OemMETA(OemMETA&&) = delete;
    OemMETA& operator=(OemMETA&&) = delete;

	public:
		/** Constucts OemMETA object
     * @param[in] dBusIntf - D-Bus handler
     * @param[in] mctp_eid - MCTP EID of remote host firmware
     * @param[in] platformManager - MC Platform manager
     * @param[in] invoker - invoker handler
     * @param[in] platformHandler - platformHandler handler
		 */

		explicit OemMETA(const pldm::utils::DBusHandler *dBusIntf, 
        const pldm::utils::DBusHandler* dbusHandler, uint8_t mctp_eid,
				pldm::responder::Invoker& invoker,
        pldm::responder::platform::Handler* platformHandler,
				pldm::platform_mc::Manager* platformManager
		) : dbusHandler(dbusHandler), mctp_fd(mctp_fd), invoker(invoker), platformManager(platformManager), 
				platformHandler(platformHandler)
		{
			configurationDiscovery =
					std::make_unique<pldm::ConfigurationDiscoveryHandler>(&dbusHandler);

			platformHandler->registerEventHandlers(PLDM_OEM_EVENT_CLASS_0xFB,
					{kpldm::platform_mc::oem_meta::process

			registerHandler();
		}

    /** @brief MCTP configurations handler*/
		std::unique_ptr<pldm::requester::ConfigurationDiscoveryHandler> configurationDiscovery{};

	private:
    void registerHandler()
    {
			invoker.registerHandler(
					PLDM_OEM, std::make_unique<pldm::responder::oem_meta::FileIOHandler>(
						configurationDiscovery.get()));
    }

    /** @brief D-Bus handler */
    const pldm::utils::DBusHandler* dbusHandler = nullptr;

    /** @brief MCTP EID of remote host firmware */
    uint8_t mctp_eid;

    /** @brief Object to the invoker class*/
		pldm::responder::Invoker& invoker;

    /** @brief MC Platform manager*/
		pldm::platform_mc::Manager* platformManager = nullptr;
}
} // namespace oem_meta
} // namespace pldm
