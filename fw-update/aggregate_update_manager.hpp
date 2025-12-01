#pragma once

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

    /**
     * @brief Constructor for AggregateUpdateManager
     *
     * @param[in] event - Reference to the PLDM daemon's main event loop
     * @param[in] handler - PLDM request handler
     * @param[in] instanceIdDb - Reference to the instance ID database
     * @param[in] descriptorMap - Descriptor map for the update manager
     * @param[in] downstreamDescriptorMap - Downstream device identifiers
     * @param[in] componentInfoMap - Component information map for the update
     * manager
     */
    explicit AggregateUpdateManager(
        Event& event,
        pldm::requester::Handler<pldm::requester::Request>& handler,
        InstanceIdDb& instanceIdDb, const DescriptorMap& descriptorMap,
        const DownstreamDescriptorMap& downstreamDescriptorMap,
        const ComponentInfoMap& componentInfoMap) :
        UpdateManager(event, handler, instanceIdDb, descriptorMap,
                      downstreamDescriptorMap, componentInfoMap)
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
                           const pldm_msg* request, size_t reqMsgLen) override;

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
                             const std::string& updateObjPath);

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
    void eraseUpdateManager(const SoftwareIdentifier& softwareIdentifier);

    /**
     * @brief Erase UpdateManager instances that satisfy a given predicate
     *
     * This function iterates through the stored UpdateManager instances and
     * removes those that satisfy the provided predicate function. It also
     * removes the associated descriptor and component information maps.
     *
     * @param[in] predicate - A function that takes a SoftwareIdentifier and
     * returns true if the corresponding UpdateManager should be erased
     */
    void eraseUpdateManagerIf(
        std::function<bool(const SoftwareIdentifier&)>&& predicate);

  private:
    /**
     * @brief Map of UpdateManager instances keyed by software identifier
     */
    std::map<SoftwareIdentifier, std::unique_ptr<UpdateManager>> updateManagers;

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
