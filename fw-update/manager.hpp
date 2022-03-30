#pragma once

#include "libpldm/requester/pldm.h"

#include "activation.hpp"
#include "common/types.hpp"
#include "common/utils.hpp"
#include "config.hpp"
#include "device_inventory.hpp"
#include "device_updater.hpp"
#include "firmware_inventory.hpp"
#include "inventory_manager.hpp"
#include "pldmd/dbus_impl_requester.hpp"
#include "requester/handler.hpp"
#include "update_manager.hpp"

#include <unordered_map>
#include <vector>

namespace pldm
{

namespace fw_update
{

using namespace pldm::dbus_api;

/** @class Manager
 *
 * This class handles all the aspects of the PLDM FW update specification for
 * the MCTP devices
 */
class Manager
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
     *  @param[in] event - reference to PLDM daemon's main event loop
     *  @param[in] handler - PLDM request handler
     *  @param[in] requester - Managing instance ID for PLDM requests
     *  @param[in] fwUpdateConfigFile - Config file for firmware update
     */
    explicit Manager(Event& event,
                     requester::Handler<requester::Request>& handler,
                     Requester& requester,
                     const std::filesystem::path& fwUpdateConfigFile) :
        inventoryMgr(handler, requester,
                     std::bind_front(&Manager::createInventory, this),
                     descriptorMap, componentInfoMap),
        updateManager(event, handler, requester, descriptorMap,
                      componentInfoMap),
        deviceInventoryManager(pldm::utils::DBusHandler::getBus(),
                               deviceInventoryInfo),
        fwInventoryManager(pldm::utils::DBusHandler::getBus(), fwInventoryInfo,
                           componentInfoMap)

    {
        parseConfig(fwUpdateConfigFile, deviceInventoryInfo, fwInventoryInfo,
                    componentNameMapInfo);
    }

    /** @brief Discover MCTP endpoints that support the PLDM firmware update
     *         specification and create component name information for creating
     *         message registry entries.
     *
     *  @param[in] mctpInfos - <EID, UUID> for every MCTP endpoint
     */
    void handleMCTPEndpoints(const MctpInfos& mctpInfos)
    {
        inventoryMgr.discoverFDs(mctpInfos);
        for (const auto& [eid, uuid] : mctpInfos)
        {
            if (componentNameMapInfo.contains(uuid))
            {
                componentNameMap[eid] = componentNameMapInfo[uuid];
            }
        }
    }

    /** @brief Create device and firmware inventory based on the firmware update
     *         config file and firmware inventory commands
     *
     *  @param[in] eid - MCTP endpoint
     *  @param[in] uuid - MCTP UUID
     */
    void createInventory(EID eid, UUID uuid)
    {
        auto objectPath = deviceInventoryManager.createEntry(uuid);
        if (componentInfoMap.contains(eid))
        {
            fwInventoryManager.createEntry(eid, uuid, objectPath);
        }
    }

    /** @brief Handle PLDM request for the commands in the FW update
     *         specification
     *
     *  @param[in] eid - Remote MCTP Endpoint ID
     *  @param[in] command - PLDM command code
     *  @param[in] request - PLDM request message
     *  @param[in] requestLen - PLDM request message length
     *
     *  @return PLDM response message
     */
    Response handleRequest(mctp_eid_t eid, Command command,
                           const pldm_msg* request, size_t reqMsgLen)
    {
        return updateManager.handleRequest(eid, command, request, reqMsgLen);
    }

  private:
    /** @brief Descriptor information of all the discovered MCTP endpoints */
    DescriptorMap descriptorMap;

    /** @brief Component information of all the discovered MCTP endpoints */
    ComponentInfoMap componentInfoMap;

    /** @brief PLDM firmware inventory manager */
    InventoryManager inventoryMgr;

    /** @brief PLDM firmware update manager */
    UpdateManager updateManager;

    /** @brief Config info to create D-Bus device inventory */
    DeviceInventoryInfo deviceInventoryInfo;

    /** @brief Config info to create D-Bus firmware inventory */
    FirmwareInventoryInfo fwInventoryInfo;

    /** @brief Config info to create message registry entries for fw update */
    ComponentNameMapInfo componentNameMapInfo;

    /** @brief Component information to create message registries */
    ComponentNameMap componentNameMap;

    /** @brief Device inventory D-Bus object manager */
    device_inventory::Manager deviceInventoryManager;

    /** @brief Firmware inventory D-Bus object manager */
    fw_inventory::Manager fwInventoryManager;
};

} // namespace fw_update

} // namespace pldm
