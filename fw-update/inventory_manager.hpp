#pragma once

#include "libpldm/requester/pldm.h"

#include "common/types.hpp"
#include "pldmd/dbus_impl_requester.hpp"
#include "requester/handler.hpp"

namespace pldm
{

namespace fw_update
{

using CreateInventoryCallBack = std::function<void(EID, UUID)>;
using MctpEidMap = std::unordered_map<EID, UUID>;

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
     *  @param[in] requester - Managing instance ID for PLDM requests
     *  @param[in] createInventoryCallBack - Optional callback function to
     *                                       create device/firmware inventory
     *  @param[out] descriptorMap - Populate the firmware identifers for the
     *                              FDs managed by the BMC.
     *  @param[out] componentInfoMap - Populate the component info for the FDs
     *                                 managed by the BMC.
     */
    explicit InventoryManager(
        pldm::requester::Handler<pldm::requester::Request>& handler,
        pldm::dbus_api::Requester& requester,
        CreateInventoryCallBack createInventoryCallBack,
        DescriptorMap& descriptorMap, ComponentInfoMap& componentInfoMap) :
        handler(handler),
        requester(requester), createInventoryCallBack(createInventoryCallBack),
        descriptorMap(descriptorMap), componentInfoMap(componentInfoMap)
    {}

    /** @brief Discover the firmware identifiers and component details of FDs
     *
     *  Inventory commands QueryDeviceIdentifiers and GetFirmwareParmeters
     *  commands are sent to every FD and the response is used to populate
     *  the firmware identifiers and component details of the FDs.
     *
     *  @param[in] eids - MCTP endpoint ID of the FDs
     */
    void discoverFDs(const MctpInfos& mctpInfos);

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

  private:
    /** @brief Send GetFirmwareParameters command request
     *
     *  @param[in] eid - Remote MCTP endpoint
     */
    void sendGetFirmwareParametersRequest(mctp_eid_t eid);

    /** @brief PLDM request handler */
    pldm::requester::Handler<pldm::requester::Request>& handler;

    /** @brief D-Bus API for managing instance ID*/
    pldm::dbus_api::Requester& requester;

    /** @brief Optional callback function to create device/firmware inventory*/
    CreateInventoryCallBack createInventoryCallBack;

    /** @brief Device identifiers of the managed FDs */
    DescriptorMap& descriptorMap;

    /** @brief Component information needed for the update of the managed FDs */
    ComponentInfoMap& componentInfoMap;

    /** @brief MCTP endpoint to MCTP UUID mapping*/
    MctpEidMap mctpEidMap;
};

} // namespace fw_update

} // namespace pldm
