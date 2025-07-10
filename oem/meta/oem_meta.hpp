#pragma once

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
    OemMETA(const OemMETA&) = delete;
    OemMETA& operator=(const OemMETA&) = delete;
    OemMETA(OemMETA&&) = delete;
    OemMETA& operator=(OemMETA&&) = delete;

  public:
    /** Constucts OemMETA object
     * @param[in] dBusIntf - D-Bus handler
     */

    explicit OemMETA(const pldm::utils::DBusHandler* dbusHandler);

    pldm::requester::oem_meta::ConfigurationDiscoveryHandler*
        getMctpConfigurationHandler() const;

  private:
    /** @brief MCTP configurations handler*/
    std::shared_ptr<pldm::requester::oem_meta::ConfigurationDiscoveryHandler>
        configurationDiscovery{};
};

} // namespace oem_meta
} // namespace pldm
