#pragma once

#include "libpldmresponder/platform.hpp"
#include "event/oem_event_manager.hpp"
#include "requester/configuration_discovery_handler.hpp"

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
     */

    explicit OemMETA(const pldm::utils::DBusHandler* dbusHandler,
                     pldm::responder::platform::Handler* platformHandler);

    pldm::requester::oem_meta::ConfigurationDiscoveryHandler*
        getMctpConfigurationHandler() const;

  private:
    void registerOemEventHandler(
        pldm::responder::platform::Handler* platformHandler);

    /** @brief MCTP configurations handler*/
    std::shared_ptr<pldm::requester::oem_meta::ConfigurationDiscoveryHandler>
        configurationDiscovery{};

    std::shared_ptr<oem_meta::OemEventManager> oemEventManager{};
};

} // namespace oem_meta
} // namespace pldm
