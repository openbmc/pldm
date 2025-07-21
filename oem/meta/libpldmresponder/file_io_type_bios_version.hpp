#pragma once

#include "common/utils.hpp"
#include "file_io_by_type.hpp"
#include "oem/meta/utils.hpp"

namespace pldm::responder::oem_meta
{

/** @class PowerStatusHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to set BMC's Host BIOS version based on the incoming request.
 */
class BIOSVersionHandler : public FileHandler
{
  public:
    BIOSVersionHandler() = delete;
    BIOSVersionHandler(const BIOSVersionHandler&) = delete;
    BIOSVersionHandler(BIOSVersionHandler&&) = delete;
    BIOSVersionHandler& operator=(const BIOSVersionHandler&) = delete;
    BIOSVersionHandler& operator=(BIOSVersionHandler&&) = delete;

    explicit BIOSVersionHandler(
        pldm_tid_t tid,
        std::map<pldm::dbus::ObjectPath, pldm::oem_meta::MctpEndpoint>&
            configurations,
        const pldm::utils::DBusHandler* dBusIntf) :
        tid(tid), configurations(configurations), dBusIntf(dBusIntf)
    {}

    ~BIOSVersionHandler() = default;

    /** @brief Method to set Host's BIOS version.
     *  @param[in] data - BIOS version raw data.
     *  @return  PLDM completion code.
     */
    int write(const message& data) override;

  private:
    /** @brief Method to convert raw data to BIOS string Dbus value.
     *  @param[in] data - Incoming data.
     *  @return - BIOS version Dbus value
     */
    pldm::utils::PropertyValue convertToBIOSVersionStr(const message& data);

    /** @brief Method to check raw data integrity.
     *  @param[in] data - List of Incoming raw data.
     *  @return - PLDM completion code.
     */
    int checkDataIntegrity(const message& data);

    /** @brief The requester's TID */
    pldm_tid_t tid = 0;

    /** @brief Configurations searched by ConfigurationDiscoveryHandler */
    std::map<pldm::dbus::ObjectPath /*configDbusPath*/,
             pldm::oem_meta::MctpEndpoint>& configurations;

    /** @brief D-Bus Interface object*/
    const pldm::utils::DBusHandler* dBusIntf;
};

} // namespace pldm::responder::oem_meta
