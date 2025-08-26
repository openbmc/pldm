#pragma once

#include "update_manager.hpp"
#include "item_based_update.hpp"

namespace pldm::fw_update
{
class ItemBasedUpdateManager : public UpdateManagerIntf
{
  public:
    ItemBasedUpdateManager() = delete;
    ItemBasedUpdateManager(const ItemBasedUpdateManager&) = delete;
    ItemBasedUpdateManager(ItemBasedUpdateManager&&) = delete;
    ItemBasedUpdateManager& operator=(const ItemBasedUpdateManager&) = delete;
    ItemBasedUpdateManager& operator=(ItemBasedUpdateManager&&) = delete;
    ~ItemBasedUpdateManager() = default;

    explicit ItemBasedUpdateManager(
        Event& event,
        pldm::requester::Handler<pldm::requester::Request>& handler,
        InstanceIdDb& instanceIdDb, const Descriptors& descriptors,
        const ComponentInfo& componentInfo,
        const std::string& objectPath) :
        UpdateManagerIntf(event, handler, instanceIdDb),
        descriptors(descriptors), componentInfo(componentInfo),
        update(pldm::utils::DBusHandler::getBus(),
               objectPath, this)
    {}

    Response handleRequest(mctp_eid_t eid, uint8_t command,
                           const pldm_msg* request, size_t reqMsgLen) override;
    void updateDeviceCompletion(mctp_eid_t eid, bool status) override;
    void updateActivationProgress() override;

    DeviceUpdaterInfos associatePkgToDevices(
        const FirmwareDeviceIDRecords& fwDeviceIDRecords,
        const DescriptorMap& descriptorMap,
        TotalComponentUpdates& totalNumComponentUpdates);

  private:
    /** @brief Device identifiers of the managed FDs */
    const Descriptors& descriptors;
    /** @brief Component information needed for the update of the managed FDs */
    const ComponentInfo& componentInfo;

    ItemBasedUpdate update;

    decltype(std::chrono::steady_clock::now()) startTime;
    std::unique_ptr<sdeventplus::source::Defer> updateDeferHandler;
};
} // namespace pldm::fw_update
