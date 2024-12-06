#pragma once

#include "common/utils.hpp"
#include "mctp_endpoint_discovery.hpp"

#include <stdexcept>

namespace pldm
{

struct MctpEndpoint
{
    uint64_t address;
    uint64_t EndpointId;
    uint64_t bus;
    std::string name;
    std::optional<std::string> iana;
};

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
        const pldm::utils::DBusHandler* dBusIntf) :
        dBusIntf(dBusIntf)
    {}

    /** @brief Discover configuration associated with the new MCTP endpoints.
     *
     *  @param[in] newMctpInfos - information of discovered MCTP endpoint
     */
    void handleMctpEndpoints(const MctpInfos& newMctpInfos);

    /** @brief Remove configuration associated with the removed MCTP endpoints.
     *
     *  @param[in] removedMctpInfos - the removed MCTP endpoints
     */
    void handleRemovedMctpEndpoints(const MctpInfos& removedMctpInfos);

    /** @brief Get existing configurations.
     *
     *  @return The configurations.
     */
    std::map<std::string, MctpEndpoint>& getConfigurations();

  private:
    /** @brief Search for associated configuration for the MctpInfo.
     *
     *  @param[in] mctpInfo - information of discovered MCTP endpoint
     */
    void searchConfigurationFor(MctpInfo mctpInfo);

    /** @brief Remove configuration associated with the removed MCTP endpoint.
     *
     *  @param[in] eid - endpoint Id of the removed MCTP Endpoint
     */
    void removeConfigByEid(uint8_t eid);

    /** @brief The configuration contains Dbus path and the MCTP endpoint
     * information.
     */
    std::map<std::string /*configDbusPath*/, MctpEndpoint> configurations;

    /** @brief D-Bus Interface object*/
    const pldm::utils::DBusHandler* dBusIntf;
};

} // namespace pldm
