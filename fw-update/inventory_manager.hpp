#pragma once

#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "firmware_inventory_manager.hpp"
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
     *  @param[in] mctpInfos - List of MCTP endpoint information
     */
    void discoverFDs(const MctpInfos& mctpInfos);

    /** @brief Remove the firmware identifiers and component details of FDs
     *
     *  This function removes the firmware identifiers, component details and
     *  downstream device identifiers of the FDs managed by the BMC.
     *
     *  @param[in] mctpInfos - List of MCTP endpoint information
     */
    void removeFDs(const MctpInfos& mctpInfos);

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
    void queryDeviceIdentifiers(mctp_eid_t eid, const pldm_msg* response,
                                size_t respMsgLen);

    /** @brief Handler for QueryDownstreamDevices command response
     *
     *  @param[in] eid - Remote MCTP endpoint
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - Response message length
     */
    void queryDownstreamDevices(mctp_eid_t eid, const pldm_msg* response,
                                size_t respMsgLen);

    /** @brief Handler for QueryDownstreamIdentifiers command response
     *
     *  @param[in] eid - Remote MCTP endpoint
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - Response message length
     */
    void queryDownstreamIdentifiers(mctp_eid_t eid, const pldm_msg* response,
                                    size_t respMsgLen);

    /** @brief Handler for GetDownstreamFirmwareParameters command response
     *
     *  @param[in] eid - Remote MCTP endpoint
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - Response message length
     */
    void getDownstreamFirmwareParameters(
        mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen);

    /** @brief Handler for GetFirmwareParameters command response
     *
     *  Handling the response of GetFirmwareParameters command and create
     *  software version D-Bus objects.
     *
     *  @param[in] eid - Remote MCTP endpoint
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - Response message length
     */
    void getFirmwareParameters(mctp_eid_t eid, const pldm_msg* response,
                               size_t respMsgLen);

    /** @brief Update the configuration bindings
     *        from the Entity Manager
     * @param[in] configurations - Configuration bindings
     */
    void updateConfigurations(const Configurations& configurations);

  private:
    /** @brief Refresh the inventory path of the FD
     *
     *  @param[in] mctpInfo - MCTP endpoint information
     */
    void refreshInventoryPath(const MctpInfo& mctpInfo);

    /**
     * @brief Sends QueryDeviceIdentifiers request
     *
     * @param[in] eid - Remote MCTP endpoint
     */
    void sendQueryDeviceIdentifiersRequest(mctp_eid_t eid);

    /**
     * @brief Sends QueryDownstreamDevices request
     *
     * @param[in] eid - Remote MCTP endpoint
     */
    void sendQueryDownstreamDevicesRequest(mctp_eid_t eid);

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
    void sendQueryDownstreamIdentifiersRequest(
        mctp_eid_t eid, uint32_t dataTransferHandle,
        enum transfer_op_flag transferOperationFlag);

    /**
     * @brief Sends QueryDownstreamFirmwareParameters request
     *
     * @param[in] eid - Remote MCTP endpoint
     * @param[in] dataTransferHandle - Data transfer handle
     * @param[in] transferOperationFlag - Transfer operation flag
     */
    void sendGetDownstreamFirmwareParametersRequest(
        mctp_eid_t eid, uint32_t dataTransferHandle,
        const enum transfer_op_flag transferOperationFlag);

    /**
     * @brief composite Downstream Device Name by combining the descriptors
     *
     * @param[in] eid - Remote MCTP endpoint
     * @param[in] index - Downstream device index
     * @param[in] descriptors - Descriptors of the downstream device
     */
    void updateDownstreamDeviceName(const eid& eid,
                                    const DownstreamDeviceIndex& index,
                                    const Descriptors& descriptors);

    /**
     * @brief update Firmware Device Name
     *
     * @param[in] eid - Remote MCTP endpoint
     * @param[in] descriptors - Descriptors of the firmware device
     */
    void updateFirmwareDeviceName(pldm::eid eid,
                                  const Descriptors& descriptors);

    /**
     * @brief Obtain the device name from the descriptors
     *
     * @param[in] descriptors - Descriptors of the device
     *
     * @return SoftwareName - The Device name, empty if not found
     */
    SoftwareName obtainDeviceNameFromDescriptors(
        const Descriptors& descriptors);

    /** @brief Send GetFirmwareParameters command request
     *
     *  @param[in] eid - Remote MCTP endpoint
     */
    void sendGetFirmwareParametersRequest(mctp_eid_t eid);

    /** @brief PLDM request handler */
    pldm::requester::Handler<pldm::requester::Request>& handler;

    /** @brief Instance ID database for managing instance ID*/
    InstanceIdDb& instanceIdDb;

    /** @brief Device identifiers of the managed FDs */
    DescriptorMap& descriptorMap;

    /** @brief Firmware Device names of the managed FDs */
    std::map<eid, SoftwareName> firmwareDeviceNameMap;

    /** @brief Downstream Devices names of the managed FDs */
    std::map<std::tuple<eid, DownstreamDeviceIndex>, SoftwareName>
        downstreamDeviceNameMap;

    /** @brief Dbus Inventory Item Manager */
    FirmwareInventoryManager firmwareInventoryManager;

    /** @brief Downstream Device identifiers of the managed FDs */
    DownstreamDescriptorMap& downstreamDescriptorMap;

    /** @brief Component information needed for the update of the managed FDs */
    ComponentInfoMap& componentInfoMap;

    /** @brief Configuration bindings from Entity Manager */
    Configurations configurations;
};

} // namespace fw_update

} // namespace pldm
