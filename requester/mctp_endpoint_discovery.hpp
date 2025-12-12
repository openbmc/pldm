#pragma once

#include "common/transport.hpp"
#include "common/types.hpp"
#include "common/utils.hpp"

#include <libpldm/pldm.h>

#include <sdbusplus/bus/match.hpp>
#include <xyz/openbmc_project/Common/UUID/common.hpp>
#include <xyz/openbmc_project/MCTP/Endpoint/client.hpp>

#include <filesystem>
#include <initializer_list>
#include <vector>

using MCTPEndpoint = sdbusplus::common::xyz::openbmc_project::mctp::Endpoint;
using CommonUUID = sdbusplus::common::xyz::openbmc_project::common::UUID;

class TestMctpDiscovery;

namespace pldm
{

const std::string emptyUUID = "00000000-0000-0000-0000-000000000000";
constexpr const char* MCTPService = "au.com.codeconstruct.MCTP1";
constexpr const char* MCTPPath = "/au/com/codeconstruct/mctp1";
constexpr const char* MCTPInterfaceCC = "au.com.codeconstruct.MCTP.Endpoint1";
constexpr const char* MCTPConnectivityProp = "Connectivity";
constexpr const char* inventorySubtreePathStr =
    "/xyz/openbmc_project/inventory/system";

const std::vector<std::string> interfaceFilter = {
    "xyz.openbmc_project.Configuration.MCTPI2CTarget",
    "xyz.openbmc_project.Configuration.MCTPI3CTarget"};

/** @class MctpDiscoveryHandlerIntf
 *
 * This abstract class defines the APIs for MctpDiscovery class has common
 * interface to execute function from different Manager Classes
 */
class MctpDiscoveryHandlerIntf
{
  public:
    virtual void handleMctpEndpoints(const TerminusInfos& mctpInfos) = 0;
    virtual void handleRemovedMctpEndpoints(const TerminusInfos& mctpInfos) = 0;
    virtual void updateMctpEndpointAvailability(const MctpInfo& mctpInfo,
                                                Availability availability) = 0;
    /** @brief Allocate or get TID for an MCTP endpoint
     *
     *  @param[in] mctpInfo - MCTP endpoint information
     *  @return TID if successful, nullopt otherwise
     */
    virtual std::optional<pldm_tid_t> allocateOrGetTid(
        const MctpInfo& mctpInfo) = 0;

    /** @brief Get transport for TID mapping
     *
     *  @return Pointer to PldmTransport instance
     */
    virtual PldmTransport* getTransport() = 0;

    /** @brief Get Active EIDs.
     *
     *  @param[in] addr - MCTP address of terminus
     *  @param[in] terminiNames - MCTP terminus name
     */
    virtual std::optional<mctp_eid_t> getActiveEidByName(
        const std::string& terminusName) = 0;

    virtual void handleConfigurations(const Configurations& /*configurations*/)
    {}
    virtual ~MctpDiscoveryHandlerIntf() {}
};

class MctpDiscovery
{
  public:
    MctpDiscovery() = delete;
    MctpDiscovery(const MctpDiscovery&) = delete;
    MctpDiscovery(MctpDiscovery&&) = delete;
    MctpDiscovery& operator=(const MctpDiscovery&) = delete;
    MctpDiscovery& operator=(MctpDiscovery&&) = delete;
    ~MctpDiscovery() = default;

    /** @brief Constructs the MCTP Discovery object to handle discovery of
     *         MCTP enabled devices
     *
     *  @param[in] bus - reference to systemd bus
     *  @param[in] list - initializer list to the MctpDiscoveryHandlerIntf
     */
    explicit MctpDiscovery(
        sdbusplus::bus_t& bus,
        std::initializer_list<MctpDiscoveryHandlerIntf*> list);

    /** @brief reference to the systemd bus */
    sdbusplus::bus_t& bus;

    /** @brief Used to watch for new MCTP endpoints */
    sdbusplus::bus::match_t mctpEndpointAddedSignal;

    /** @brief Used to watch for the removed MCTP endpoints */
    sdbusplus::bus::match_t mctpEndpointRemovedSignal;

    /** @brief Used to watch for new MCTP endpoints */
    sdbusplus::bus::match_t mctpEndpointPropChangedSignal;

    /** @brief List of handlers need to notify when new MCTP
     * Endpoint is Added/Removed */
    std::vector<MctpDiscoveryHandlerIntf*> handlers;

    /** @brief The existing MCTP endpoints */
    TerminusInfos existingMctpInfos;

    /** @brief Callback function when the propertiesChanged D-Bus
     * signal is triggered for MCTP endpoint's properties.
     *
     *  @param[in] msg - Data associated with subscribed signal
     */
    void propertiesChangedCb(sdbusplus::message_t& msg);

    /** @brief Callback function when MCTP endpoints addedInterface
     * D-Bus signal raised.
     *
     *  @param[in] msg - Data associated with subscribed signal
     */
    void discoverEndpoints(sdbusplus::message_t& msg);

    /** @brief Callback function when MCTP endpoint removedInterface
     * D-Bus signal raised.
     *
     *  @param[in] msg - Data associated with subscribed signal
     */
    void removeEndpoints(sdbusplus::message_t& msg);

    /** @brief Helper function to invoke registered handlers for
     *  the added MCTP endpoints
     *
     *  @param[in] mctpInfos - information of discovered MCTP endpoints
     */
    void handleMctpEndpoints(const TerminusInfos& mctpInfos);

    /** @brief Helper function to invoke registered handlers for
     *  the removed MCTP endpoints
     *
     *  @param[in] mctpInfos - information of removed MCTP endpoints
     */
    void handleRemovedMctpEndpoints(const TerminusInfos& mctpInfos);

    /** @brief Helper function to invoke registered handlers for
     *  updating the availability status of the MCTP endpoint
     *
     *  @param[in] mctpInfo - information of the target endpoint
     *  @param[in] availability - new availability status
     */
    void updateMctpEndpointAvailability(const MctpInfo& mctpInfo,
                                        Availability availability);

    /** @brief Get list of TerminusInfos in MCTP control interface.
     *
     *  @param[in] mctpInfoMap - information of discovered MCTP endpoints
     *  and the availability status of each endpoint
     */
    void getMctpInfos(std::map<MctpInfo, Availability>& mctpInfoMap);

    /** @brief Get list of new TerminusInfos in addedInterace D-Bus signal
     * message.
     *
     *  @param[in] msg - addedInterace D-Bus signal message
     *  @param[in] mctpInfos - information of added MCTP endpoints
     */
    void getAddedMctpInfos(sdbusplus::message_t& msg, TerminusInfos& mctpInfos);

    /** @brief Add new TerminusInfos to existingMctpInfos.
     *
     *  @param[in] mctpInfos - information of new MCTP endpoints
     */
    void addToExistingMctpInfos(const TerminusInfos& mctpInfos);

    /** @brief Erase the removed MCTP endpoint from existingMctpInfos.
     *
     *  @param[in] mctpInfos - the remaining MCTP endpoints
     *  @param[out] removedInfos - the removed MCTP endpoints
     */
    void removeFromExistingMctpInfos(TerminusInfos& mctpInfos,
                                     TerminusInfos& removedInfos);

    friend class ::TestMctpDiscovery;

  private:
    /** @brief Get MCTP Endpoint D-Bus Properties in the
     *         `xyz.openbmc_project.MCTP.Endpoint` D-Bus interface
     *
     *  @param[in] service - the MCTP service name
     *  @param[in] path - the MCTP endpoints object path
     *
     *  @return tuple of Network Index, Endpoint ID and MCTP message types
     */
    MctpEndpointProps getMctpEndpointProps(const std::string& service,
                                           const std::string& path);

    /** @brief Get Endpoint UUID from `UUID` D-Bus property in the
     *         `xyz.openbmc_project.Common.UUID` D-Bus interface.
     *
     *  @param[in] service - the MCTP service name
     *  @param[in] path - the MCTP endpoints object path
     *
     *  @return Endpoint UUID
     */
    UUID getEndpointUUIDProp(const std::string& service,
                             const std::string& path);

    /** @brief Get Endpoint Availability status from `Connectivity` D-Bus
     *         property in the `au.com.codeconstruct.MCTP.Endpoint1` D-Bus
     *         interface.
     *
     *  @param[in] path - the MCTP endpoints object path
     *
     *  @return Availability status: true if active false if inactive
     */
    Availability getEndpointConnectivityProp(const std::string& path);

    static constexpr uint8_t mctpTypePLDM = 1;

    /** @brief Construct the MCTP reactor object path
     *
     *  @param[in] mctpInfo - information of discovered MCTP endpoint
     *
     *  @return the MCTP reactor object path
     */
    std::string constructMctpReactorObjectPath(const MctpInfo& mctpInfo);

    /** @brief Search for associated configuration for the MctpInfo.
     *
     *  @param[in] mctpInfo - information of discovered MCTP endpoint
     */
    void searchConfigurationFor(const pldm::utils::DBusHandler& handler,
                                MctpInfo& mctpInfo);

    /** @brief Remove configuration associated with the removed MCTP endpoint.
     *
     *  @param[in] removedInfos - the removed MCTP endpoints
     */
    void removeConfigs(const TerminusInfos& removedInfos);

    /** @brief An internal helper function to get the name property from the
     * properties
     * @param[in] properties - the properties of the D-Bus object
     * @return the name property
     */
    std::string getNameFromProperties(const utils::PropertyMap& properties);

    /** @brief The configuration contains D-Bus path and the MCTP endpoint
     * information.
     */
    Configurations configurations;
};

} // namespace pldm
