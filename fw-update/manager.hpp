#pragma once

#include "aggregate_update_manager.hpp"
#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "inventory_manager.hpp"
#include "requester/handler.hpp"
#include "requester/mctp_endpoint_discovery.hpp"

namespace pldm
{

namespace fw_update
{

/** @class Manager
 *
 * This class handles all the aspects of the PLDM FW update specification for
 * the MCTP devices
 */
class Manager : public pldm::MctpDiscoveryHandlerIntf
{
  public:
    Manager() = delete;
    Manager(const Manager&) = delete;
    Manager(Manager&&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager& operator=(Manager&&) = delete;
    ~Manager() override = default;

    /**
     * @brief Constructor for the PLDM Firmware Update Manager
     *
     * @param[in] dbusHandler - Pointer to the D-Bus handler used for querying
     * inventory and board paths
     * @param[in] event - Reference to the io_context event object for
     * asynchronous operations
     * @param[in] handler - Reference to the PLDM request handler for processing
     * PLDM requests
     * @param[in] instanceIdDb - Reference to the InstanceId database for
     * managing PLDM instance IDs
     * @param[in] registry - Registry that tells which MCTP endpoint a PeerId
     * stands for. It must outlive the Manager.
     */
    explicit Manager(const pldm::utils::DBusHandler* dbusHandler, Event& event,
                     requester::Handler<requester::Request>& handler,
                     pldm::InstanceIdDb& instanceIdDb,
                     const EndpointRegistry& registry) :
        registry(registry),
        updateManager(event, handler, instanceIdDb, registry, descriptorMap,
                      componentInfoMap),
        inventoryMgr(dbusHandler, handler, instanceIdDb, descriptorMap,
                     downstreamDescriptorMap, componentInfoMap, configurations,
                     updateManager, registry)
    {}

    /** @brief Discover the firmware devices behind the added endpoints
     *
     *  @param[in] endpoints - the added endpoints and their PeerIds
     */
    void handleEndpoints(
        const std::vector<DiscoveredEndpoint>& endpoints) override
    {
        inventoryMgr.discoverFDs(endpoints);
    }

    /** @brief Remove the firmware devices behind the removed endpoints
     *
     *  @param[in] endpoints - the removed endpoints and their PeerIds
     */
    void handleRemovedEndpoints(
        const std::vector<DiscoveredEndpoint>& endpoints) override
    {
        inventoryMgr.removeFDs(endpoints);
    }

    /** @brief Same as handleEndpoints(), for callers that only have the
     *         MctpInfo. Endpoints that have no PeerId are ignored.
     *
     *  @param[in] mctpInfos - information of discovered MCTP endpoints
     */
    void handleMctpEndpoints(const MctpInfos& mctpInfos) override
    {
        handleEndpoints(withPeerIds(mctpInfos));
    }

    /** @brief Helper function to invoke registered handlers for
     *         the updated EM configurations
     *
     *  @param[in] configurations - updated EM configurations
     */
    void handleConfigurations(const Configurations& configurations) override
    {
        this->configurations = configurations;
    }

    /** @brief Same as handleRemovedEndpoints(), for callers that only have
     *         the MctpInfo. Endpoints that have no PeerId are ignored.
     *
     *  @param[in] mctpInfos - information of removed MCTP endpoints
     */
    void handleRemovedMctpEndpoints(const MctpInfos& mctpInfos) override
    {
        handleRemovedEndpoints(withPeerIds(mctpInfos));
    }

    /** @brief Helper function to invoke registered handlers for
     *  updating the availability status of the MCTP endpoint
     *
     *  @param[in] mctpInfo - information of the target endpoint
     *  @param[in] availability - new availability status
     */
    void updateMctpEndpointAvailability(const MctpInfo&, Availability) override
    {
        return;
    }

    /** @brief Handle PLDM request for the commands in the FW update
     *         specification
     *
     *  @param[in] peer - PeerId of the remote MCTP endpoint
     *  @param[in] command - PLDM command code
     *  @param[in] request - PLDM request message
     *  @param[in] requestLen - PLDM request message length
     *  @return PLDM response message
     */
    Response handleRequest(PeerId peer, Command command,
                           const pldm_msg* request, size_t reqMsgLen)
    {
        return updateManager.handleRequest(peer, command, request, reqMsgLen);
    }

    /** @brief Get the PeerId of the active endpoint of a terminus. fw-update
     *         does not name its devices after termini, so there is none.
     */
    std::optional<PeerId> getActivePeerIdByName(const std::string&) override
    {
        return std::nullopt;
    }

  private:
    std::vector<DiscoveredEndpoint> withPeerIds(
        const MctpInfos& mctpInfos) const
    {
        std::vector<DiscoveredEndpoint> endpoints;
        for (const auto& mctpInfo : mctpInfos)
        {
            if (auto peer = registry.peerOf(toEndpoint(mctpInfo)))
            {
                endpoints.push_back({*peer, mctpInfo});
            }
        }
        return endpoints;
    }

    /** @brief Registry of the PeerIds of the discovered MCTP endpoints */
    const EndpointRegistry& registry;

    /** Descriptor information of all the discovered MCTP endpoints */
    DescriptorMap descriptorMap;

    /** Downstream descriptor information of all the discovered MCTP endpoints
     */
    DownstreamDescriptorMap downstreamDescriptorMap;

    /** Component information of all the discovered MCTP endpoints */
    ComponentInfoMap componentInfoMap;

    /** Configuration bindings from the Entity Manager */
    Configurations configurations;

    /** @brief PLDM firmware update manager */
    AggregateUpdateManager updateManager;

    /** @brief PLDM firmware inventory manager */
    InventoryManager inventoryMgr;
};

} // namespace fw_update

} // namespace pldm
