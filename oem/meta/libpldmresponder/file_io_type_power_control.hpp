#pragma once

#include "common/utils.hpp"
#include "file_io_by_type.hpp"
#include "oem/meta/requester/configuration_discovery_handler.hpp"

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
    ~PowerControlHandler() = default;

    explicit PowerControlHandler(
        uint8_t tid, std::map<std::string, pldm::requester::oem_meta::MctpEndpoint>& configurations,
        const pldm::utils::DBusHandler* dBusIntf) :
        tid(tid), configurations(configurations), dBusIntf(dBusIntf)
    {}

    int write(const message& data) override;
    int read(struct pldm_oem_meta_file_io_read_resp* data) override
    {
        (void)data; // Unused
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

  private:
    /** @brief Method to get the slot number who sent the request.
     *  @return - Corresponding slot number.
     */
    std::string getSlotNumber();

    /** @brief Method to get the Dbus Path of the board containing the Endpoint.
     *  @param[in] tid - The target endpoint's TID.
     *  @return - The board's Dbus path.
     */
    const std::string getDbusPathOfBoardContainingTheEndpoint(uint8_t tid);

    /** @brief Method to get the Dbus path of the configuration searched by
     * ConfigurationDiscoveryHandler.
     *  @param[in] tid - The target endpoint's TID.
     *  @return - The configuration's Dbus path.
     */
    const std::string& getConfigurationPathByTid(uint8_t tid);

    /** @brief The requester's TID */
    uint8_t tid = 0;

    /** @brief Configurations searched by ConfigurationDiscoveryHandler */
    std::map<std::string /*configDbusPath*/, pldm::requester::oem_meta::MctpEndpoint>& configurations;

    /** @brief D-Bus Interface object*/
    const pldm::utils::DBusHandler* dBusIntf;
};

} // namespace pldm::responder::oem_meta
