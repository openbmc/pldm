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

    explicit OemMETA(const pldm::utils::DBusHandler* dbusHandler)
    {
        configurationDiscovery = std::make_shared<
            pldm::requester::oem_meta::ConfigurationDiscoveryHandler>(
            dbusHandler);
    }

    pldm::requester::oem_meta::ConfigurationDiscoveryHandler*
        getMctpConfigurationHandler() const
    {
        return configurationDiscovery.get();
    }

  private:
    /** @brief MCTP configurations handler*/
    std::shared_ptr<pldm::requester::oem_meta::ConfigurationDiscoveryHandler>
        configurationDiscovery{};
};

} // namespace oem_meta
} // namespace pldm
