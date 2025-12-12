#pragma once

#include "activation.hpp"
#include "aggregate_update_manager.hpp"
#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "device_updater.hpp"
#include "inventory_manager.hpp"
#include "requester/handler.hpp"
#include "requester/mctp_endpoint_discovery.hpp"

#include <unordered_map>
#include <vector>

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
    ~Manager() = default;

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
     */
    explicit Manager(const pldm::utils::DBusHandler* dbusHandler, Event& event,
                     requester::Handler<requester::Request>& handler,
                     pldm::InstanceIdDb& instanceIdDb) :
        updateManager(event, handler, instanceIdDb, descriptorMap,
                      componentInfoMap),
        handler(handler),
        inventoryMgr(dbusHandler, handler, instanceIdDb, descriptorMap,
                     downstreamDescriptorMap, componentInfoMap, configurations,
                     updateManager)
    {}

    /** @brief Helper function to invoke registered handlers for
     *         the added MCTP endpoints
     *
     *  @param[in] mctpInfos - information of discovered MCTP endpoints
     */
    void handleMctpEndpoints(const TerminusInfos& mctpInfos) override
    {
        for (const auto& [tid, mctpInfo] : mctpInfos)
        {
            auto eid = std::get<pldm::eid>(mctpInfo);
            auto network = std::get<NetworkId>(mctpInfo);

            if (auto* transport = handler.getTransport())
            {
                transport->mapTid(tid, network, eid);
            }
        }
        inventoryMgr.discoverFDs(mctpInfos);
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

    /** @brief Helper function to invoke registered handlers for
     *         the removed MCTP endpoints
     *
     *  @param[in] mctpInfos - information of removed MCTP endpoints
     */
    void handleRemovedMctpEndpoints(const TerminusInfos& mctpInfos) override
    {
        for (const auto& [tid, mctpInfo] : mctpInfos)
        {
            if (auto* transport = handler.getTransport())
            {
                transport->unmapTid(tid);
            }
        }
        inventoryMgr.removeFDs(mctpInfos);
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
     *  @param[in] eid - Remote MCTP Endpoint ID
     *  @param[in] command - PLDM command code
     *  @param[in] request - PLDM request message
     *  @param[in] requestLen - PLDM request message length
     *  @return PLDM response message
     */
    Response handleRequest(mctp_eid_t eid, Command command,
                           const pldm_msg* request, size_t reqMsgLen)
    {
        return updateManager.handleRequest(eid, command, request, reqMsgLen);
    }

    /** @brief Get Active EIDs.
     *
     *  @param[in] addr - MCTP address of terminus
     *  @param[in] terminiNames - MCTP terminus name
     */
    std::optional<mctp_eid_t> getActiveEidByName(const std::string&) override
    {
        return std::nullopt;
    }

    std::optional<pldm_tid_t> allocateOrGetTid(
        const MctpInfo& /*mctpInfo*/) override
    {
        // FW update doesn't manage TIDs
        return std::nullopt;
    }

    PldmTransport* getTransport() override
    {
        return handler.getTransport();
    }

  private:
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

    /** @brief Reference to the PLDM request handler */
    requester::Handler<requester::Request>& handler;

    /** @brief PLDM firmware inventory manager */
    InventoryManager inventoryMgr;
};

} // namespace fw_update

} // namespace pldm
