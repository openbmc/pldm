#pragma once

#include "common/utils.hpp"
#include "oem/meta/utils.hpp"
#include "requester/mctp_endpoint_discovery.hpp"

namespace pldm::requester::oem_meta
{

class ConfigurationDiscoveryHandler : public MctpDiscoveryHandlerIntf
{
  public:
    ConfigurationDiscoveryHandler() = delete;
    ConfigurationDiscoveryHandler(const ConfigurationDiscoveryHandler&) =
        delete;
    ConfigurationDiscoveryHandler(ConfigurationDiscoveryHandler&&) = delete;
    ConfigurationDiscoveryHandler& operator=(
        const ConfigurationDiscoveryHandler&) = delete;
    ConfigurationDiscoveryHandler& operator=(ConfigurationDiscoveryHandler&&) =
        delete;
    ~ConfigurationDiscoveryHandler() = default;

    explicit ConfigurationDiscoveryHandler(
        const pldm::utils::DBusHandler* dBusIntf) : dBusIntf(dBusIntf)
    {}

    /** @brief Discover configuration associated with the new MCTP endpoints.
     *
     *  @param[in] newMctpInfos - information of discovered MCTP endpoint
     */
    void handleMctpEndpoints([[maybe_unused]] const MctpInfos& newMctpInfos) {}

    /** @brief Remove configuration associated with the removed MCTP endpoints.
     *
     *  @param[in] removedMctpInfos - the removed MCTP endpoints
     */
    void handleRemovedMctpEndpoints(
        [[maybe_unused]] const MctpInfos& removedMctpInfos)
    {}

    void updateMctpEndpointAvailability(
        [[maybe_unused]] const MctpInfo& mctpInfo,
        [[maybe_unused]] Availability availability)
    {}
    /** @brief Get Active EIDs.
     *
     *  @param[in] addr - MCTP address of terminus
     *  @param[in] terminiNames - MCTP terminus name
     */
    std::optional<mctp_eid_t> getActiveEidByName(
        [[maybe_unused]] const std::string& terminusName)
    {
        return std::nullopt;
    }

    /** @brief Get existing configurations.
     *
     *  @return The configurations.
     */
    std::map<std::string, pldm::oem_meta::MctpEndpoint>& getConfigurations(void)
    {
        return configurations;
    }

  private:
    /** @brief Search for associated configuration for the MctpInfo.
     *
     *  @param[in] mctpInfo - information of discovered MCTP endpoint
     */
    void searchConfigurationFor([[maybe_unused]] MctpInfo mctpInfo) {}

    /** @brief Append to configurations if Endpoint Id is matched.
     *
     *  @param[in] targetEid - discovered MCTP endpoint Id
     *  @param[in] configPath - the Dbus path of a configuration
     *  @param[in] serviceMap - the map contains the service's information who
     * expose the configuration
     */
    void appendConfigIfEidMatch(
        [[maybe_unused]] uint8_t targetEid,
        [[maybe_unused]] const std::string& configPath,
        [[maybe_unused]] const pldm::utils::MapperServiceMap& serviceMap)
    {}

    /** @brief Parse MctpEndpoint from the response of GetAll method.
     *
     *  @param[in] response - Response data of GetAll method
     *
     *  @return Parsed MctpEndpoint object.
     */
    pldm::oem_meta::MctpEndpoint parseMctpEndpointFromResponse(
        [[maybe_unused]] const pldm::utils::PropertyMap& response)
    {
        return pldm::oem_meta::MctpEndpoint{};
    }

    /** @brief Append to configuration if the MctpEndpoint's EID matches
     * targetEid.
     *
     *  @param[in] targetEid - The target EID
     *  @param[in] configPath - Discovered configuration's Dbus path
     *  @param[in] endpoint - The configuration's MctpEndpoint information.
     */
    void appendIfEidMatch(
        [[maybe_unused]] uint8_t targetEid,
        [[maybe_unused]] const std::string& configPath,
        [[maybe_unused]] const pldm::oem_meta::MctpEndpoint& endpoint)
    {}

    /** @brief Remove configuration associated with the removed MCTP endpoint.
     *
     *  @param[in] eid - endpoint Id of the removed MCTP Endpoint
     */
    void removeConfigByEid([[maybe_unused]] uint8_t eid) {}

    /** @brief The configuration contains Dbus path and the MCTP endpoint
     * information.
     */
    std::map<std::string /*configDbusPath*/, pldm::oem_meta::MctpEndpoint>
        configurations;

    /** @brief D-Bus Interface object*/
    [[maybe_unused]] const pldm::utils::DBusHandler* dBusIntf;
};

} // namespace pldm::requester::oem_meta
