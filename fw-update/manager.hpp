#pragma once

#include "activation.hpp"
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
    explicit Manager(Event& event,
                     requester::Handler<requester::Request>& handler,
                     pldm::InstanceIdDb& instanceIdDb) :
        inventoryMgr(handler, instanceIdDb, descriptorMap, componentInfoMap),
        updateManager(event, handler, instanceIdDb, descriptorMap,
                      componentInfoMap)
    {}

    /** @brief Discover MCTP endpoints that support the PLDM firmware update
     *         specification
     *
     *  @param[in] mctpInfos - Array of MctpInfo
     *
     *  @return return PLDM_SUCCESS on success and PLDM_ERROR otherwise
     */
    void handleMctpEndpoints(const MctpInfos& mctpInfos)
    {
        std::vector<mctp_eid_t> eids;
        for (auto& mctpInfo : mctpInfos)
        {
            eids.emplace_back(std::get<0>(mctpInfo));
        }

        inventoryMgr.discoverFDs(eids);
    }

    /** @brief Removed MCTP endpoints that support the PLDM firmware update
     *         specification
     *
     *  @param[in] mctpInfos - Array of MctpInfo
     *
     *  @return return PLDM_SUCCESS on success and PLDM_ERROR otherwise
     */
    void handleRemovedMctpEndpoints([[maybe_unused]] const MctpInfos& mctpInfos)
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

  private:
    /** Descriptor information of all the discovered MCTP endpoints */
    DescriptorMap descriptorMap;

    /** Component information of all the discovered MCTP endpoints */
    ComponentInfoMap componentInfoMap;

    /** @brief PLDM firmware inventory manager */
    InventoryManager inventoryMgr;

    /** @brief PLDM firmware update manager */
    UpdateManager updateManager;
};

} // namespace fw_update

} // namespace pldm
