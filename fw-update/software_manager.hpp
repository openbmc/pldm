#pragma once

#include "common/types.hpp"
#include "device_dedicated_updater.hpp"

namespace pldm::fw_update
{

using Context = sdbusplus::async::context;
using ObjectPath = pldm::dbus::ObjectPath;

class SoftwareManager
{
  public:
    SoftwareManager() = delete;
    SoftwareManager(Context& ctx, Event& event,
                    pldm::requester::Handler<pldm::requester::Request>& handler,
                    InstanceIdDb& instanceIdDb) :
        event(event), handler(handler), instanceIdDb(instanceIdDb), ctx(ctx)
    {}
    SoftwareManager(const SoftwareManager&) = delete;
    SoftwareManager(SoftwareManager&&) = delete;
    SoftwareManager& operator=(const SoftwareManager&) = delete;
    SoftwareManager& operator=(SoftwareManager&&) = delete;
    ~SoftwareManager() = default;

    void createSoftwareEntry(
        const SoftwareIdentifier& softwareIdentifier,
        const SoftwareName& softwareName, const std::string& activeVersion,
        const Descriptors& descriptors, const ComponentInfo& componentInfo);

    void refreshInventoryPath(const eid& eid, const InventoryPath& path);

    Response handleRequest(mctp_eid_t eid, Command command,
                           const pldm_msg* request, size_t reqMsgLen);

    Event& event;               //!< reference to PLDM daemon's main event loop
    pldm::requester::Handler<pldm::requester::Request>& handler;
    InstanceIdDb& instanceIdDb; //!< reference to an InstanceIdDb

  private:
    std::string getVersionId(const std::string& version);

    Context& ctx;

    const ObjectPath getBoardPath(const InventoryPath& path);

    std::map<eid, InventoryPath> inventoryPathMap;

    std::map<SoftwareIdentifier, std::unique_ptr<DeviceDedicatedUpdater>>
        softwareMap;
};

} // namespace pldm::fw_update
