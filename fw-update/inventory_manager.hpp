#pragma once

#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "requester/handler.hpp"

namespace pldm
{

namespace fw_update
{

/** @class InventoryManager
 *
 *  InventoryManager class manages the software inventory of firmware devices
 *  managed by the BMC. It discovers the firmware identifiers and the component
 *  details of the FD. Firmware identifiers, component details and update
 *  capabilities of FD are populated by the InventoryManager and is used for the
 *  firmware update of the FDs.
 */
class InventoryManager
{
  public:
    InventoryManager() = delete;
    InventoryManager(const InventoryManager&) = delete;
    InventoryManager(InventoryManager&&) = delete;
    InventoryManager& operator=(const InventoryManager&) = delete;
    InventoryManager& operator=(InventoryManager&&) = delete;
    ~InventoryManager() = default;

    /** @brief Constructor
     *
     *  @param[in] handler - PLDM request handler
     *  @param[in] instanceIdDb - Managing instance ID for PLDM requests
     *  @param[out] descriptorMap - Populate the firmware identifiers for the
     *                              FDs managed by the BMC.
     *  @param[out] downstreamDescriptorMap - Populate the downstream
     *                                        identifiers for the FDs managed
     *                                        by the BMC.
     *  @param[out] componentInfoMap - Populate the component info for the FDs
     *                                 managed by the BMC.
     */
    explicit InventoryManager(
        pldm::requester::Handler<pldm::requester::Request>& handler,
        InstanceIdDb& instanceIdDb, DescriptorMap& descriptorMap,
        DownstreamDescriptorMap& downstreamDescriptorMap,
        ComponentInfoMap& componentInfoMap) :
        handler(handler), instanceIdDb(instanceIdDb),
        descriptorMap(descriptorMap),
        downstreamDescriptorMap(downstreamDescriptorMap),
        componentInfoMap(componentInfoMap)
    {}

    /** @brief Discover the firmware identifiers and component details of FDs
     *
     *  Inventory commands QueryDeviceIdentifiers and GetFirmwareParmeters
     *  commands are sent to every FD and the response is used to populate
     *  the firmware identifiers and component details of the FDs.
     *
     *  @param[in] eids - MCTP endpoint ID of the FDs
     */
    void discoverFDs(const std::vector<mctp_eid_t>& eids);
    exec::task<int> discoverFDsTask();

    /** @brief Handler for QueryDeviceIdentifiers command response
     *
     *  The response of the QueryDeviceIdentifiers is processed and firmware
     *  identifiers of the FD is updated. GetFirmwareParameters command request
     *  is sent to the FD.
     *
     *  @param[in] eid - Remote MCTP endpoint
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - Response message length
     */
    exec::task<int> parseQueryDeviceIdentifiersResponse(
        mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen);

    /** @brief Starts firmware discovery flow
     *
     *  @param[in] eid - Remote MCTP endpoint
     */
    exec::task<int> startFirmwareDiscoveryFlow(mctp_eid_t eid);

    /** @brief Send QueryDeviceIdentifiers command request
     *
     *  @param[in] eid - Remote MCTP endpoint
     */
    exec::task<int> queryDeviceIdentifiers(mctp_eid_t eid);

    /** @brief Send GetFirmwareParameters command request
     *
     *  @param[in] eid - Remote MCTP endpoint
     *  @param[in] messageError - message error
     *  @param[in] resolution - recommended resolution
     */
    exec::task<int> getFirmwareParameters(mctp_eid_t eid);

    /** @brief Handler for QueryDownstreamDevices command response
     *
     *  @param[in] eid - Remote MCTP endpoint
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - Response message length
     */
    exec::task<int> parseQueryDownstreamDevicesResponse(
        mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen);

    /** @brief Handler for QueryDownstreamIdentifiers command response
     *
     *  @param[in] eid - Remote MCTP endpoint
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - Response message length
     */
    exec::task<int> parseQueryDownstreamIdentifiersResponse(
        mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen);

    /** @brief Handler for GetDownstreamFirmwareParameters command response
     *
     *  @param[in] eid - Remote MCTP endpoint
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - Response message length
     */
    exec::task<int> parseGetDownstreamFirmwareParametersResponse(
        mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen);

    /** @brief Handler for GetFirmwareParameters command response
     *
     *  Handling the response of GetFirmwareParameters command and create
     *  software version D-Bus objects.
     *
     *  @param[in] eid - Remote MCTP endpoint
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - Response message length
     *
     */
    exec::task<int> parseGetFWParametersResponse(
        mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen);

  private:
    /**
     * @brief Sends QueryDownstreamDevices request
     *
     * @param[in] eid - Remote MCTP endpoint
     */
    exec::task<int> queryDownstreamDevices(mctp_eid_t eid);

    /**
     * @brief Sends QueryDownstreamIdentifiers request
     *
     * The request format is defined at Table 16 – QueryDownstreamIdentifiers
     * command format in DSP0267_1.1.0
     *
     * @param[in] eid - Remote MCTP endpoint
     * @param[in] dataTransferHandle - Data transfer handle
     * @param[in] transferOperationFlag - Transfer operation flag
     */
    virtual exec::task<int> queryDownstreamIdentifiers(
        mctp_eid_t eid, uint32_t dataTransferHandle,
        enum transfer_op_flag transferOperationFlag);

    /**
     * @brief Sends QueryDownstreamFirmwareParameters request
     *
     * @param[in] eid - Remote MCTP endpoint
     * @param[in] dataTransferHandle - Data transfer handle
     * @param[in] transferOperationFlag - Transfer operation flag
     */
    virtual exec::task<int> getDownstreamFirmwareParameters(
        mctp_eid_t eid, uint32_t dataTransferHandle,
        const enum transfer_op_flag transferOperationFlag);

    /**
     * @brief Sends a PLDM message over MCTP and receives the response.
     *
     * @param[in] eid - The MCTP endpoint ID to which the request is sent.
     * @param[in] request - The PLDM request message to be sent.
     * @param[out] responseMsg - Pointer to the received PLDM response message.
     * @param[out] responseLen - Length of the received response message.
     *
     * @return A coroutine task that returns an integer indicating the result
     *         of the operation. Returns PLDM_SUCCESS on success, PLDM_ERROR
     * otherwise
     */
    exec::task<int> sendRecvPldmMsgOverMctp(mctp_eid_t eid, Request& request,
                                            const pldm_msg** responseMsg,
                                            size_t* responseLen);

    /** @brief PLDM request handler */
    pldm::requester::Handler<pldm::requester::Request>& handler;

    /** @brief Instance ID database for managing instance ID*/
    InstanceIdDb& instanceIdDb;

    /** @brief Device identifiers of the managed FDs */
    DescriptorMap& descriptorMap;

    /** @brief Downstream Device identifiers of the managed FDs */
    DownstreamDescriptorMap& downstreamDescriptorMap;

    /** @brief Component information needed for the update of the managed FDs */
    ComponentInfoMap& componentInfoMap;

    /** @brief A queue of MctpEids to be discovered **/
    std::queue<std::vector<mctp_eid_t>> queuedMctpEids{};

    /** @brief coroutine handle of discoverFDsTask */
    std::optional<std::pair<exec::async_scope, std::optional<int>>>
        discoverFDsTaskHandle{};
};

} // namespace fw_update

} // namespace pldm
