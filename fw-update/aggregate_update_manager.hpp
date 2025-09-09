#pragma once

#include "item_based_update_manager.hpp"
#include "update_manager.hpp"

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

    /**
     * @brief Constructor for AggregateUpdateManager
     *
     * @param[in] event - Reference to the PLDM daemon's main event loop
     * @param[in] handler - PLDM request handler
     * @param[in] instanceIdDb - Reference to the instance ID database
     * @param[in] descriptorMap - Descriptor map for the update manager
     * @param[in] componentInfoMap - Component information map for the update
     * manager
     */
    explicit AggregateUpdateManager(
        Event& event,
        pldm::requester::Handler<pldm::requester::Request>& handler,
        InstanceIdDb& instanceIdDb, const DescriptorMap& descriptorMap,
        const ComponentInfoMap& componentInfoMap) :
        UpdateManager(event, handler, instanceIdDb, descriptorMap,
                      componentInfoMap),
        event(event), handler(handler), instanceIdDb(instanceIdDb)
    {}

    /**
     * @brief Handle PLDM requests for the aggregate update manager
     *
     * This function processes incoming PLDM requests and dispatches them to the
     * appropriate update manager based on the software identifier.
     *
     * @param[in] eid - Remote MCTP Endpoint ID
     * @param[in] command - PLDM command code
     * @param[in] request - PLDM request message
     * @param[in] reqMsgLen - PLDM request message length
     * @return PLDM response message
     */
    Response handleRequest(mctp_eid_t eid, uint8_t command,
                           const pldm_msg* request, size_t reqMsgLen) override
    {
        Response response;
        response =
            UpdateManager::handleRequest(eid, command, request, reqMsgLen);
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

    /**
     * @brief Create a new UpdateManager instance for a specific software
     * identifier
     *
     * This function creates and stores a new UpdateManager instance associated
     * with the given software identifier, along with its corresponding
     * descriptor and component information maps.
     *
     * @param[in] softwareIdentifier - The software identifier (pair of eid and
     * component identifier)
     * @param[in] descriptors - The descriptors associated with the software
     * identifier
     * @param[in] componentInfo - The component information associated with the
     * software identifier
     * @param[in] updateObjPath - The D-Bus object path for the update manager
     */
    void createUpdateManager(const SoftwareIdentifier& softwareIdentifier,
                             const Descriptors& descriptors,
                             const ComponentInfo& componentInfo,
                             const std::string& updateObjPath)
    {
        auto eid = softwareIdentifier.first;

        descriptorMap[softwareIdentifier] =
            std::make_unique<Descriptors>(descriptors);
        componentInfoMap[softwareIdentifier] =
            std::make_unique<ComponentInfo>(componentInfo);

        updateManagers[softwareIdentifier] =
            std::make_shared<ItemBasedUpdateManager>(
                eid, event, handler, instanceIdDb, updateObjPath,
                *descriptorMap[softwareIdentifier],
                *componentInfoMap[softwareIdentifier]);
    }

    /**
     * @brief Erase an existing UpdateManager instance associated with a
     * specific software identifier
     *
     * This function removes the UpdateManager instance and its associated
     * descriptor and component information maps from the internal storage based
     * on the provided software identifier.
     *
     * @param[in] softwareIdentifier - The software identifier (pair of eid and
     * component identifier)
     */
    void eraseUpdateManager(const SoftwareIdentifier& softwareIdentifier)
    {
        updateManagers.erase(softwareIdentifier);
        descriptorMap.erase(softwareIdentifier);
        componentInfoMap.erase(softwareIdentifier);
    }

    /**
     * @brief Query an existing UpdateManager instance associated with a
     * specific software identifier
     *
     * This function retrieves the UpdateManager instance and its associated
     * descriptor and component information maps from the internal storage based
     * on the provided software identifier.
     *
     * @param[in] softwareIdentifier - The software identifier (pair of eid and
     * component identifier)
     * @return A shared pointer to the UpdateManager instance, or nullptr if not
     * found
     */
    auto queryUpdateManager(const SoftwareIdentifier& softwareIdentifier)
        -> std::shared_ptr<ItemBasedUpdateManager>
    {
        auto it = updateManagers.find(softwareIdentifier);
        if (it != updateManagers.end())
        {
            return it->second;
        }
        return nullptr;
    }

  private:
    /**
     * @brief Reference to the PLDM daemon's main event loop
     */
    Event& event;

    /**
     * @brief Reference to the PLDM request handler
     */
    pldm::requester::Handler<pldm::requester::Request>& handler;

    /**
     * @brief Reference to the instance ID database
     */
    InstanceIdDb& instanceIdDb;

    /**
     * @brief Map of UpdateManager instances keyed by software identifier
     */
    std::map<SoftwareIdentifier, std::shared_ptr<ItemBasedUpdateManager>>
        updateManagers;

    /**
     * @brief Map of descriptor maps keyed by software identifier
     */
    std::map<SoftwareIdentifier, std::unique_ptr<Descriptors>> descriptorMap;

    /**
     * @brief Map of component information maps keyed by software identifier
     */
    std::map<SoftwareIdentifier, std::unique_ptr<ComponentInfo>>
        componentInfoMap;
};

} // namespace pldm::fw_update
