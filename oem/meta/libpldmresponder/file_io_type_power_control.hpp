#pragma once

#include "common/utils.hpp"
#include "file_io_by_type.hpp"
#include "oem/meta/utils.hpp"

namespace pldm::responder::oem_meta
{
/** @class PowerControlHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to handle incoming sled cycle request from Hosts
 */
class PowerControlHandler : public FileHandler
{
  public:
    PowerControlHandler() = delete;
    PowerControlHandler(const PowerControlHandler&) = delete;
    PowerControlHandler(PowerControlHandler&&) = delete;
    PowerControlHandler& operator=(const PowerControlHandler&) = delete;
    PowerControlHandler& operator=(PowerControlHandler&&) = delete;

    explicit PowerControlHandler(
        pldm_tid_t tid,
        std::map<pldm::dbus::ObjectPath, pldm::oem_meta::MctpEndpoint>& configurations,
        const pldm::utils::DBusHandler* dBusIntf) :
        tid(tid), configurations(configurations), dBusIntf(dBusIntf)
    {}

    ~PowerControlHandler() = default;

    /** @brief Method to add handler for write-file command
     * 				 "SLED_CYCLE, 12V-CYCLE and DC-cycle" to let
     * 				 Host can trigger power control to the system.
     *				 - Option:
     *				  	- 0x00: Sled-cycle
     *				  	- 0x01: Slot 12V-cycle
     *				  	- 0x02: Slot DC-cycle
     *  @param[in] data - APML alert raw data.
     *  @return  PLDM completion code.
     */
    int write(const message& data) override;

  private:
    /** @brief The requester's TID */
    pldm_tid_t tid = 0;

    /** @brief Configurations searched by ConfigurationDiscoveryHandler */
    std::map<pldm::dbus::ObjectPath /*configDbusPath*/, pldm::oem_meta::MctpEndpoint>&
        configurations;

    /** @brief D-Bus Interface object*/
    const pldm::utils::DBusHandler* dBusIntf;
};

} // namespace pldm::responder::oem_meta
