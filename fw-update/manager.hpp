#pragma once

#include "activation.hpp"
#include "aggregate_update_manager.hpp"
#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "device_updater.hpp"
#include "inventory_manager.hpp"
#include "requester/handler.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "update_manager.hpp"

#include <unordered_map>
#include <vector>

namespace pldm
{

namespace fw_update
{

using Context = sdbusplus::async::context;

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

    /** @brief Constructor
     *
     *  @param[in] handler - PLDM request handler
     */
    explicit Manager(Context& ctx, Event& event,
                     requester::Handler<requester::Request>& handler,
                     pldm::InstanceIdDb& instanceIdDb) :
        updateManager(event, handler, instanceIdDb, descriptorMap,
                      downstreamDescriptorMap, componentInfoMap),
        inventoryMgr(ctx, handler, instanceIdDb, descriptorMap,
                     downstreamDescriptorMap, componentInfoMap, updateManager)
    {}

    /** @brief Helper function to invoke registered handlers for
     *         the added MCTP endpoints
     *
     *  @param[in] mctpInfos - information of discovered MCTP endpoints
     */
    void handleMctpEndpoints(const MctpInfos& mctpInfos) override
    {
        inventoryMgr.discoverFDs(mctpInfos);
    }

    void handleConfigurations(const Configurations& configurations) override
    {
        inventoryMgr.updateConfigurations(configurations);
    }

    /** @brief Helper function to invoke registered handlers for
     *         the removed MCTP endpoints
     *
     *  @param[in] mctpInfos - information of removed MCTP endpoints
     */
    void handleRemovedMctpEndpoints(const MctpInfos&) override
    {
        return;
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

  private:
    /** Descriptor information of all the discovered MCTP endpoints */
    DescriptorMap descriptorMap;

    /** Downstream descriptor information of all the discovered MCTP endpoints
     */
    DownstreamDescriptorMap downstreamDescriptorMap;

    /** Component information of all the discovered MCTP endpoints */
    ComponentInfoMap componentInfoMap;

    /** @brief PLDM update manager */
    AggregateUpdateManager updateManager;

    /** @brief PLDM firmware inventory manager */
    InventoryManager inventoryMgr;
};

} // namespace fw_update

} // namespace pldm
