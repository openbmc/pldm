#pragma once

#include "common/utils.hpp"
#include "oem_meta_file_io_by_type.hpp"
#include "requester/configuration_discovery_handler.hpp"

namespace pldm::responder::oem_meta
{

/** @class PowerStatusHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to set BMC's Host ACPIPowerStatus based on the incoming request.
 */
class PowerStatusHandler : public FileHandler
{
  public:
    PowerStatusHandler() = delete;
    PowerStatusHandler(const PowerStatusHandler&) = delete;
    PowerStatusHandler(PowerStatusHandler&&) = delete;
    PowerStatusHandler& operator=(const PowerStatusHandler&) = delete;
    PowerStatusHandler& operator=(PowerStatusHandler&&) = delete;
    ~PowerStatusHandler() = default;

    explicit PowerStatusHandler(
        uint8_t tid, std::map<std::string, MctpEndpoint>& configurations,
        const pldm::utils::DBusHandler* dBusIntf) :
        tid(tid),
        configurations(configurations), dBusIntf(dBusIntf)
    {}

    /** @brief Method to set Host's ACPIPowerStatus.
     *  @param[in] data - ACPIPowerStatus raw data.
     *  @return  PLDM completion code.
     */
    int write(const message& data) override;

    int read(const message&) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

  private:
    /** @brief Method to convert raw data to ACPIPowerStatus Dbus value.
     *  @param[in] rawData - ACPIPowerStatus raw data.
     *  @return - ACPIPowerStatus Dbus value
     */
    pldm::utils::PropertyValue convertToACPIDbusValue(uint8_t rawData);

    /** @brief Method to check raw data integrity.
     *  @param[in] data - List of Incoming raw data.
     *  @return - PLDM completion code.
     */
    int checkDataIntegrity(const message& data);

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
    std::map<std::string /*configDbusPath*/, MctpEndpoint>& configurations;

    /** @brief D-Bus Interface object*/
    const pldm::utils::DBusHandler* dBusIntf;
};

} // namespace pldm::responder::oem_meta
