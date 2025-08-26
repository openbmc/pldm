#pragma once

#include "item_based_update_manager.hpp"


namespace pldm::fw_update
{

class AggregateUpdateManager : public UpdateManager
{
  public:
    AggregateUpdateManager() = delete;
    AggregateUpdateManager(const AggregateUpdateManager&) = delete;
    AggregateUpdateManager(AggregateUpdateManager&&) = delete;
    AggregateUpdateManager& operator=(const AggregateUpdateManager&) = delete;
    AggregateUpdateManager& operator=(AggregateUpdateManager&&) = delete;
    ~AggregateUpdateManager() = default;

    explicit AggregateUpdateManager(
        Event& event,
        pldm::requester::Handler<pldm::requester::Request>& handler,
        InstanceIdDb& instanceIdDb, const DescriptorMap& descriptorMap,
        const ComponentInfoMap& componentInfoMap) :
        UpdateManager(event, handler, instanceIdDb, descriptorMap,
                      componentInfoMap), event(event),
        handler(handler), instanceIdDb(instanceIdDb)
    {}

    Response handleRequest(mctp_eid_t eid, uint8_t command,
                           const pldm_msg* request, size_t reqMsgLen) override
    {
        Response response;
        response = UpdateManager::handleRequest(eid, command, request, reqMsgLen);
        auto responseMsg = new (response.data()) pldm_msg;
        if (responseMsg->payload[0] != PLDM_FWUP_COMMAND_NOT_EXPECTED)
        {
            return response;
        }
        for (auto& [_, updateManager] : updateManagers)
        {
            response = updateManager->handleRequest(eid, command, request, reqMsgLen);
            if (responseMsg->payload[0] != PLDM_FWUP_COMMAND_NOT_EXPECTED)
            {
                return response;
            }
        }
        return response;
    }

    void insertOrAssignUpdateManager(
        const SoftwareIdentifier& softwareIdentifier,
        const Descriptors& descriptors,
        const ComponentInfo& componentInfo)
    {
        auto eid = softwareIdentifier.first;
        updateManagers[softwareIdentifier] = std::make_shared<UpdateManager>(
            event, handler, instanceIdDb, DescriptorMap{{eid, descriptors}},
            ComponentInfoMap{{eid, componentInfo}});
    }

    void eraseUpdateManager(
        const SoftwareIdentifier& softwareIdentifier)
    {
        updateManagers.erase(softwareIdentifier);
    }

    auto queryUpdateManager(
        const SoftwareIdentifier& softwareIdentifier) -> std::shared_ptr<UpdateManager>
    {
        auto it = updateManagers.find(softwareIdentifier);
        if (it != updateManagers.end())
        {
            return it->second;
        }
        return nullptr;
    }

  private:
    Event& event; //!< reference to PLDM daemon's main event loop
    pldm::requester::Handler<pldm::requester::Request>& handler; //!< PLDM request handler
    InstanceIdDb& instanceIdDb; //!< reference to an
    std::map<SoftwareIdentifier, std::shared_ptr<UpdateManager>> updateManagers;
};

}