#include "aggregate_update_manager.hpp"

namespace pldm::fw_update
{

Response AggregateUpdateManager::handleRequest(
    mctp_eid_t eid, uint8_t command, const pldm_msg* request, size_t reqMsgLen)
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
        response =
            updateManager->handleRequest(eid, command, request, reqMsgLen);
        if (responseMsg->payload[0] != PLDM_FWUP_COMMAND_NOT_EXPECTED)
        {
            return response;
        }
    }
    return response;
}

void AggregateUpdateManager::createUpdateManager(
    const SoftwareIdentifier& /*softwareIdentifier*/,
    const Descriptors& /*descriptors*/, const ComponentInfo& /*componentInfo*/,
    const std::string& /*updateObjPath*/)
{
    // TODO: implement after ItemUpdateManager is ready
}

void AggregateUpdateManager::eraseUpdateManager(
    const SoftwareIdentifier& softwareIdentifier)
{
    updateManagers.erase(softwareIdentifier);
    descriptorMap.erase(softwareIdentifier);
    componentInfoMap.erase(softwareIdentifier);
}

void AggregateUpdateManager::eraseUpdateManagerIf(
    std::function<bool(const SoftwareIdentifier&)>&& predicate)
{
    for (auto it = updateManagers.begin(); it != updateManagers.end();)
    {
        if (predicate(it->first))
        {
            descriptorMap.erase(it->first);
            componentInfoMap.erase(it->first);
            it = updateManagers.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace pldm::fw_update
