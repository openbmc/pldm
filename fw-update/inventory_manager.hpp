#pragma once

#include "aggregate_update_manager.hpp"
#include "common/endpoint.hpp"
#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "firmware_inventory_manager.hpp"
#include "requester/endpoint_registry.hpp"
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
     *  @param[in] registry - Tells which MCTP endpoint a PeerId stands for
     */
    explicit InventoryManager(
        const pldm::utils::DBusHandler* dbusHandler,
        pldm::requester::Handler<pldm::requester::Request>& handler,
        InstanceIdDb& instanceIdDb, DescriptorMap& descriptorMap,
        DownstreamDescriptorMap& downstreamDescriptorMap,
        ComponentInfoMap& componentInfoMap,
        const Configurations& configurations,
        AggregateUpdateManager& updateManager,
        const EndpointRegistry& registry) :
        handler(handler), instanceIdDb(instanceIdDb),
        descriptorMap(descriptorMap),
        downstreamDescriptorMap(downstreamDescriptorMap),
        componentInfoMap(componentInfoMap), configurations(configurations),
        registry(registry),
        firmwareInventoryManager(dbusHandler, configurations, updateManager,
                                 registry)
    {}

    /** @brief Discover the firmware identifiers and component details of FDs
     *
     *  Inventory commands QueryDeviceIdentifiers and GetFirmwareParmeters
     *  commands are sent to every FD and the response is used to populate
     *  the firmware identifiers and component details of the FDs. The FDs are
     *  keyed by the PeerId of their endpoint.
     *
     *  @param[in] endpoints - The MCTP endpoints and their PeerIds
     */
    void discoverFDs(const std::vector<DiscoveredEndpoint>& endpoints);

    /** @brief Remove the firmware identifiers and component details of FDs
     *
     *  This function removes the firmware identifiers, component details and
     *  downstream device identifiers of the FDs managed by the BMC.
     *
     *  @param[in] endpoints - The MCTP endpoints and their PeerIds
     */
    void removeFDs(const std::vector<DiscoveredEndpoint>& endpoints);

    /** @brief Handler for QueryDeviceIdentifiers command response
     *
     *  The response of the QueryDeviceIdentifiers is processed and firmware
     *  identifiers of the FD is updated. GetFirmwareParameters command request
     *  is sent to the FD.
     *
     *  @param[in] peer - PeerId of the remote MCTP endpoint
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - Response message length
     */
    void queryDeviceIdentifiers(PeerId peer, const pldm_msg* response,
                                size_t respMsgLen);

    /** @brief Handler for QueryDownstreamDevices command response
     *
     *  @param[in] peer - PeerId of the remote MCTP endpoint
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - Response message length
     */
    void queryDownstreamDevices(PeerId peer, const pldm_msg* response,
                                size_t respMsgLen);

    /** @brief Handler for QueryDownstreamIdentifiers command response
     *
     *  @param[in] peer - PeerId of the remote MCTP endpoint
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - Response message length
     */
    void queryDownstreamIdentifiers(PeerId peer, const pldm_msg* response,
                                    size_t respMsgLen);

    /** @brief Handler for GetDownstreamFirmwareParameters command response
     *
     *  @param[in] peer - PeerId of the remote MCTP endpoint
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - Response message length
     */
    void getDownstreamFirmwareParameters(PeerId peer, const pldm_msg* response,
                                         size_t respMsgLen);

    /** @brief Handler for GetFirmwareParameters command response
     *
     *  Handling the response of GetFirmwareParameters command and create
     *  software version D-Bus objects.
     *
     *  @param[in] peer - PeerId of the remote MCTP endpoint
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - Response message length
     */
    void getFirmwareParameters(PeerId peer, const pldm_msg* response,
                               size_t respMsgLen);

  private:
    /**
     * @brief Sends QueryDeviceIdentifiers request
     *
     * @param[in] peer - PeerId of the remote MCTP endpoint
     */
    void sendQueryDeviceIdentifiersRequest(PeerId peer);

    /**
     * @brief Sends QueryDownstreamDevices request
     *
     * @param[in] peer - PeerId of the remote MCTP endpoint
     */
    void sendQueryDownstreamDevicesRequest(PeerId peer);

    /**
     * @brief Sends QueryDownstreamIdentifiers request
     *
     * The request format is defined at Table 16 – QueryDownstreamIdentifiers
     * command format in DSP0267_1.1.0
     *
     * @param[in] peer - PeerId of the remote MCTP endpoint
     * @param[in] dataTransferHandle - Data transfer handle
     * @param[in] transferOperationFlag - Transfer operation flag
     */
    void sendQueryDownstreamIdentifiersRequest(
        PeerId peer, uint32_t dataTransferHandle,
        enum transfer_op_flag transferOperationFlag);

    /**
     * @brief Sends QueryDownstreamFirmwareParameters request
     *
     * @param[in] peer - PeerId of the remote MCTP endpoint
     * @param[in] dataTransferHandle - Data transfer handle
     * @param[in] transferOperationFlag - Transfer operation flag
     */
    void sendGetDownstreamFirmwareParametersRequest(
        PeerId peer, uint32_t dataTransferHandle,
        const enum transfer_op_flag transferOperationFlag);

    /**
     * @brief obtain Firmware Device Name from either configurations or
     * descriptors, if none of them is available, a default name is
     * generated.
     *
     * @param[in] peer - PeerId of the remote MCTP endpoint
     * @param[in] descriptors - Descriptors of the firmware device
     */
    void obtainFirmwareDeviceName(PeerId peer, const Descriptors& descriptors);

    /** @brief Send GetFirmwareParameters command request
     *
     *  @param[in] peer - PeerId of the remote MCTP endpoint
     */
    void sendGetFirmwareParametersRequest(PeerId peer);

    /** @brief PLDM request handler */
    pldm::requester::Handler<pldm::requester::Request>& handler;

    /** @brief Instance ID database for managing instance ID*/
    InstanceIdDb& instanceIdDb;

    /** @brief Device identifiers of the managed FDs */
    DescriptorMap& descriptorMap;

    /** @brief Firmware Device names of the managed FDs */
    std::map<PeerId, SoftwareName> firmwareDeviceNameMap;

    /** @brief Downstream Device identifiers of the managed FDs */
    DownstreamDescriptorMap& downstreamDescriptorMap;

    /** @brief Component information needed for the update of the managed FDs */
    ComponentInfoMap& componentInfoMap;

    /** @brief Configuration bindings from Entity Manager */
    const Configurations& configurations;

    /** @brief Tells which MCTP endpoint a PeerId stands for */
    const EndpointRegistry& registry;

    /** @brief Dbus Inventory Item Manager */
    FirmwareInventoryManager firmwareInventoryManager;
};

/**
 * @brief Obtain the device name from the configurations
 *
 * @param[in] configurations - Configurations from Entity Manager
 * @param[in] endpoint - Remote MCTP endpoint
 *
 * @return SoftwareName - The Device name, std::nullopt if not found
 */
std::optional<SoftwareName> obtainDeviceNameFromConfigurations(
    const Configurations& configurations, const pldm::Endpoint& endpoint);

/**
 * @brief Obtain the device name from the descriptors
 *
 * @param[in] descriptors - Descriptors of the device
 *
 * @return SoftwareName - The Device name, std::nullopt if not found
 */
std::optional<SoftwareName> obtainDeviceNameFromDescriptors(
    const Descriptors& descriptors);

} // namespace fw_update

} // namespace pldm
