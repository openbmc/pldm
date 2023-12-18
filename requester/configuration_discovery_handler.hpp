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
};

class ConfigurationDiscoveryHandler : public MctpDiscoveryHandlerIntf
{
  public:
    ConfigurationDiscoveryHandler() = delete;
    ConfigurationDiscoveryHandler(const ConfigurationDiscoveryHandler&) =
        delete;
    ConfigurationDiscoveryHandler(ConfigurationDiscoveryHandler&&) = delete;
    ConfigurationDiscoveryHandler&
        operator=(const ConfigurationDiscoveryHandler&) = delete;
    ConfigurationDiscoveryHandler&
        operator=(ConfigurationDiscoveryHandler&&) = delete;
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

    /** @brief Append to configurations if Endpoint Id is matched.
     *
     *  @param[in] targetEid - discovered MCTP endpoint Id
     *  @param[in] configPath - the Dbus path of a configuration
     *  @param[in] serviceMap - the map contains the service's information who
     * expose the configuration
     */
    void
        appendConfigIfEidMatch(uint8_t targetEid, const std::string& configPath,
                               const pldm::utils::MapperServiceMap& serviceMap);

    /** @brief Parse MctpEndpoint from the response of GetAll method.
     *
     *  @param[in] response - Response data of GetAll method
     *
     *  @return Parsed MctpEndpoint object.
     */
    MctpEndpoint
        parseMctpEndpointFromResponse(const pldm::utils::PropertyMap& response);

    /** @brief Append to configuration if the MctpEndpoint's EID matches
     * targetEid.
     *
     *  @param[in] targetEid - The target EID
     *  @param[in] configPath - Discovered configuration's Dbus path
     *  @param[in] endpoint - The configuration's MctpEndpoint information.
     */
    void appendIfEidMatch(uint8_t targetEid, const std::string& configPath,
                          const MctpEndpoint& endpoint);

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

class NoSuchPropertiesException : public std::exception
{
  public:
    NoSuchPropertiesException() = delete;
    ~NoSuchPropertiesException() = default;

    explicit NoSuchPropertiesException(
        const std::vector<std::string> properties)
    {
        std::string missingProperties{};
        for (const auto& property : properties)
        {
            missingProperties += std::string(property) + " ";
        }
        message = "Interface has no properties: " + missingProperties;
    }

    const char* what() const noexcept override
    {
        return message.c_str();
    }

  private:
    std::string message;
};

} // namespace pldm
