#pragma once

#include "libpldm/requester/pldm.h"

#include "common/types.hpp"
#include "pldmd/dbus_impl_requester.hpp"
#include "requester/handler.hpp"
#include "version.hpp"

#include <map>

namespace pldm
{

namespace fw_update
{

using namespace pldm::dbus_api;

/** @class InventoryManager
 *
 *  InventoryManager class manages the software inventory of firmware devices
 *  managed by the BMC. It discovers the firmware identifiers and the component
 *  details of the FD. Firmware identifiers, component details and update
 *  capabilities of FD are populated by the InventoryManager and is used for the
 *  firmware update of the FDs.
 *
 *  Software inventory is enumerated on the D-Bus and exposed out of band by
 *  Redfish. It implements the xyz.openbmc_project.Software.Version interface.
 *  ActiveComponentImageSetVersionString in the GetFirmwareParameters command
 *  response is used to populate the Software.Version interface. There can be
 *  multiple FDs that have the same version, but only one software version
 *  object will be created. The root D-Bus object path for software inventory is
 *  /xyz/openbmc_project/software/ and hash of the
 *  ActiveComponentImageSetVersionString is appended to the D-Bus object path.
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
     *  @param[in] requester - reference to Requester object
     *  @param[out] descriptorMap - Populate the firmware identifers for the
     *                              FDs managed by the BMC.
     *  @param[out] componentInfoMap - Populate the component info for the FDs
     *                                 managed by the BMC.
     */
    explicit InventoryManager(
        pldm::requester::Handler<pldm::requester::Request>& handler,
        Requester& requester, DescriptorMap& descriptorMap,
        ComponentInfoMap& componentInfoMap) :
        handler(handler),
        requester(requester), descriptorMap(descriptorMap),
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
    void queryDeviceIdentifiersRespHandler(mctp_eid_t eid,
                                           const pldm_msg* response,
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
    void getFirmwareParametersRespHandler(mctp_eid_t eid,
                                          const pldm_msg* response,
                                          size_t respMsgLen);

    const std::string swRootPath{"/xyz/openbmc_project/software/"};

  private:
    /** @brief Send GetFirmwareParameters command request
     *
     *  @param[in] eid - Remote MCTP endpoint
     */
    void sendGetFirmwareParametersRequest(mctp_eid_t eid);

    /** @brief PLDM request handler */
    pldm::requester::Handler<pldm::requester::Request>& handler;

    Requester& requester; //!< reference to Requester object

    /** @brief Device identifiers of the managed FDs */
    DescriptorMap& descriptorMap;

    /** @brief Component information needed for the update of the managed FDs */
    ComponentInfoMap& componentInfoMap;

    using VersionHash = std::size_t;
    /** @brief FW inventory version objects */
    std::map<VersionHash, std::unique_ptr<Version>> versionMap;
};

} // namespace fw_update

} // namespace pldm
