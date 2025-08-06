#pragma once

#include "common/types.hpp"
#include "firmware_inventory.hpp"

namespace pldm::fw_update
{

using ObjectPath = pldm::dbus::ObjectPath;

class FirmwareInventoryManager
{
  public:
    FirmwareInventoryManager() = delete;
    FirmwareInventoryManager(
        Event& event,
        pldm::requester::Handler<pldm::requester::Request>& handler,
        InstanceIdDb& instanceIdDb) :
        event(event), handler(handler), instanceIdDb(instanceIdDb)
    {}
    FirmwareInventoryManager(const FirmwareInventoryManager&) = delete;
    FirmwareInventoryManager(FirmwareInventoryManager&&) = delete;
    FirmwareInventoryManager& operator=(const FirmwareInventoryManager&) =
        delete;
    FirmwareInventoryManager& operator=(FirmwareInventoryManager&&) = delete;
    ~FirmwareInventoryManager() = default;

    void emplace(const SoftwareIdentifier& softwareIdentifier,
                 const SoftwareName& softwareName,
                 const std::string& activeVersion,
                 const Descriptors& descriptors,
                 const ComponentInfo& componentInfo);

    void removeById(const mctp_eid_t& eid);

    void refreshInventoryPath(const eid& eid, const InventoryPath& path);

    Response handleRequest(mctp_eid_t eid, Command command,
                           const pldm_msg* request, size_t reqMsgLen);

    Event& event;               //!< reference to PLDM daemon's main event loop
    pldm::requester::Handler<pldm::requester::Request>& handler;
    InstanceIdDb& instanceIdDb; //!< reference to an InstanceIdDb

  private:
    std::string getVersionId(const std::string& version);

    const ObjectPath getBoardPath(const InventoryPath& path);

    std::map<eid, InventoryPath> inventoryPathMap;

    std::map<SoftwareIdentifier, std::unique_ptr<FirmwareInventory>>
        softwareMap;
};

} // namespace pldm::fw_update
