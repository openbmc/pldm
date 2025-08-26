#include "aggregate_update_manager.hpp"

namespace pldm::fw_update
{

AggregateUpdateManager::AggregateUpdateManager(
    Event& event,
    pldm::requester::Handler<pldm::requester::Request>& handler,
    InstanceIdDb& instanceIdDb, const DescriptorMap& descriptorMap,
    const ComponentInfoMap& componentInfoMap)
    : UpdateManager(event, handler, instanceIdDb, descriptorMap, componentInfoMap),
      event(event), handler(handler), instanceIdDb(instanceIdDb)
{
}

Response AggregateUpdateManager::handleRequest(mctp_eid_t eid, uint8_t command,
                                               const pldm_msg* request, size_t reqMsgLen)
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

void AggregateUpdateManager::insertOrAssignUpdateManager(
    const SoftwareIdentifier& softwareIdentifier,
    const Descriptors& descriptors,
    const ComponentInfo& componentInfo)
{
    auto eid = softwareIdentifier.first;
    updateManagers[softwareIdentifier] = std::make_shared<UpdateManager>(
        event, handler, instanceIdDb, DescriptorMap{{eid, descriptors}},
        ComponentInfoMap{{eid, componentInfo}});
}

void AggregateUpdateManager::eraseUpdateManager(
    const SoftwareIdentifier& softwareIdentifier)
{
    updateManagers.erase(softwareIdentifier);
}

auto AggregateUpdateManager::queryUpdateManager(
    const SoftwareIdentifier& softwareIdentifier) -> std::shared_ptr<UpdateManager>
{
    auto it = updateManagers.find(softwareIdentifier);
    if (it != updateManagers.end())
    {
        return it->second;
    }
    return nullptr;
}

} // namespace pldm::fw_update
