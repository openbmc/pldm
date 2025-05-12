#pragma once

#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "requester/handler.hpp"
#include "xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

namespace pldm
{

namespace fw_update
{

using versionserver =
    sdbusplus::xyz::openbmc_project::Software::server::Version;
using definitionsserver =
    sdbusplus::xyz::openbmc_project::Association::server::Definitions;
using assetserver =
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset;

using VersionInterface = sdbusplus::server::object_t<versionserver>;
using DefinitionsInterface = sdbusplus::server::object_t<definitionsserver>;
using AssetInterface = sdbusplus::server::object_t<assetserver>;

/** @class SoftwareInventory
 *  @brief OpenBMC Software Inventory implementation.
 *  @details Exposes PLDM firmware versions on DBus.
 */
class SoftwareInventory :
    public VersionInterface,
    public DefinitionsInterface,
    public AssetInterface
{
  public:
    SoftwareInventory() = delete;
    SoftwareInventory(const SoftwareInventory&) = delete;
    SoftwareInventory& operator=(const SoftwareInventory&) = delete;
    SoftwareInventory(SoftwareInventory&&) = delete;
    SoftwareInventory& operator=(SoftwareInventory&&) = delete;

    /** @brief Constructor to put object onto bus at a dbus path.
     *  @param[in] bus - Bus to attach to.
     *  @param[in] path - Path to attach at.
     */
    SoftwareInventory(sdbusplus::bus_t& bus, const std::string& path) :
        VersionInterface(bus, path.c_str()),
        DefinitionsInterface(bus, path.c_str()),
        AssetInterface(bus, path.c_str()) {};

    /** @brief Set value of Purpose in Software.Version */
    versionserver::VersionPurpose purpose(versionserver::VersionPurpose value);

    /** @brief Set value of Version in Software.Version */
    std::string version(std::string value);
};

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

  private:
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

    /** @brief Send GetFirmwareParameters command request
     *
     *  @param[in] eid - Remote MCTP endpoint
     */
    void sendGetFirmwareParametersRequest(mctp_eid_t eid);

    /** @brief Add to software inventory
     *
     *  @param[in] eid - Remote MCTP endpoint
     *  @param[in] componentNumber - Component number
     *  @param[in] activeCompVerStr - Active component version string
     *  @param[in] pendingCompVerStr - Pending component version string
     */
    void addToSoftwareInventory(mctp_eid_t eid, int componentNumber,
                                const std::string& activeCompVerStr,
                                const std::string& pendingCompVerStr);

    /** @brief Get UUID from Entity Manager Inventory
     *
     *  @param[in] eid - Remote MCTP endpoint
     *  @param[out] UUID - UUID of the FD
     *  @return true if UUID is found, false otherwise
     */
    bool getUUID(mctp_eid_t eid, std::string& UUID);

    /** @brief Get device name from Entity Manager Inventory
     *
     *  @param[in] eid - Remote MCTP endpoint
     *  @param[in] UUID - UUID of the FD
     *  @param[out] deviceName - Device name
     */
    void getDeviceName(mctp_eid_t eid, const std::string& UUID,
                       std::string& deviceName);

    /** @brief Create software inventory path
     *
     *  @param[in] deviceName - Device name
     *  @return true if software inventory path is created, false otherwise
     */
    bool createSoftwareInventoryPath(std::string deviceName);

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

    utils::DBusHandler dbusHandler;

    /* @brief The pointer of software inventory D-Bus interface for the terminus
     */
    std::map<std::string, std::unique_ptr<SoftwareInventory>>
        softwareInventoryInterfaces;
};

} // namespace fw_update

} // namespace pldm
