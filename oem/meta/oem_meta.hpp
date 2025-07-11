#pragma once

#include "common/utils.hpp"
#include "libpldmresponder/platform.hpp"
#include "oem/meta/event/oem_event_manager.hpp"
#include "utils.hpp"

namespace pldm::oem_meta
{

/**
 * @class OemMETA
 *
 * @brief class for creating all the OEM META handlers. Only in case
 *        of OEM_META this class object will be instantiated
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
    /** @brief Constucts OemMETA object
     *  @param[in] dBusIntf - D-Bus handler
     *  @param[in] platformHandler - platformHandler handler
     */
    explicit OemMETA(const pldm::utils::DBusHandler* dbusHandler,
                     pldm::responder::platform::Handler* platformHandler);

    /** @brief return pointer to ConfigurationDiscoveryHandler for mctp endpoint
     *         discovery
     *  @param[out] pointer to ConfigurationDiscoveryHandler
     */
    ConfigurationDiscoveryHandler* getMctpConfigurationHandler() const;

  private:
    void registerOemEventHandler(
        pldm::responder::platform::Handler* platformHandler);

    std::unique_ptr<oem_meta::OemEventManager> oemEventManager{};
};

} // namespace pldm::oem_meta
