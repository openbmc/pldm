#include "item_based_update.hpp"
#include "update_manager.hpp"

#include <span>
#include <spanstream>

namespace pldm::fw_update
{

using namespace sdeventplus;
using namespace sdeventplus::source;
using namespace pldm;

namespace software = sdbusplus::xyz::openbmc_project::Software::server;

class ItemBasedUpdate;

class ItemBasedUpdateManager : public UpdateManagerBase
{
  public:
    ItemBasedUpdateManager() = delete;
    ItemBasedUpdateManager(const ItemBasedUpdateManager&) = delete;
    ItemBasedUpdateManager(ItemBasedUpdateManager&&) = delete;
    ItemBasedUpdateManager& operator=(const ItemBasedUpdateManager&) = delete;
    ItemBasedUpdateManager& operator=(ItemBasedUpdateManager&&) = delete;
    ~ItemBasedUpdateManager() = default;

    /**
     * @brief Constructor for ItemBasedUpdateManager
     *
     * @param[in] eid The MCTP EID
     * @param[in] event The event object
     * @param[in] handler The request handler
     * @param[in] instanceIdDb The instance ID database
     * @param[in] objPath The D-Bus object path
     */
    explicit ItemBasedUpdateManager(
        mctp_eid_t eid, Event& event,
        pldm::requester::Handler<pldm::requester::Request>& handler,
        InstanceIdDb& instanceIdDb, const std::string& objPath,
        const Descriptors& descriptors, const ComponentInfo& componentInfo) :
        UpdateManagerBase(event, handler, instanceIdDb), eid(eid),
        objPath(objPath), descriptors(descriptors),
        componentInfo(componentInfo),
        update(pldm::utils::DBusHandler::getBus(), objPath, this),
        activation(std::make_unique<Activation>(
            pldm::utils::DBusHandler::getBus(), objPath,
            software::Activation::Activations::Active, this))
    {}

    /**
     * @brief Handle PLDM requests for the item-based update manager
     *
     * @param[in] eid - Remote MCTP Endpoint ID
     * @param[in] command - PLDM command code
     * @param[in] request - PLDM request message
     * @param[in] reqMsgLen - PLDM request message length
     * @return PLDM response message
     */
    Response handleRequest(mctp_eid_t eid, uint8_t command,
                           const pldm_msg* request, size_t reqMsgLen);

    /**
     * @brief Process the firmware update package
     *
     * @param[in] packageData - The firmware update package data
     * @return 0 on success, negative error code on failure
     */
    int processPackage(std::span<const uint8_t> packageData);

    /**
     * @brief Send the defer request of the firmware update package
     *
     * @param[in] packageData - The firmware update package data
     * @return The D-Bus object path of the firmware update package
     */
    std::string sendProcessPackage(std::span<const uint8_t> packageData);

    /**
     * @brief Update the device completion status
     *
     * @param[in] eid - The MCTP EID of the device
     * @param[in] status - The completion status (true for success, false for
     * failure)
     */
    void updateDeviceCompletion(mctp_eid_t eid, bool status) override;

    /**
     * @brief Update the activation progress status
     */
    void updateActivationProgress() override;

    /**
     * @brief Activate the firmware update package
     */
    void activatePackage() override;

    /**
     * @brief Clear the activation information
     */
    void clearActivationInfo() override;

    /**
     * @brief Associate the firmware update package with the target device
     *
     * This function associates the firmware update package with the specified
     * target device by matching the device ID records and descriptors.
     *
     * @param[in] fwDeviceIDRecords The firmware device ID records
     * @param[in] descriptors The descriptors to match against
     *
     * @return The offset of the device ID record if found, std::nullopt
     * otherwise.
     */
    std::optional<DeviceIDRecordOffset> associatePkgToDevice(
        const FirmwareDeviceIDRecords& fwDeviceIDRecords,
        const Descriptors& descriptors);

  private:
    mctp_eid_t eid;
    std::string objPath;

    /**
     * @brief The descriptors to match against
     */
    const Descriptors& descriptors;

    /**
     * @brief The component information of the target device
     */
    const ComponentInfo& componentInfo;

    /**
     * @brief The package data stream for the firmware update
     */
    std::unique_ptr<std::ispanstream> packageDataStream;

    /**
     * @brief The item-based startUpdate implementation
     */
    ItemBasedUpdate update;

    std::unique_ptr<Activation> activation;
    std::unique_ptr<ActivationProgress> activationProgress;
    std::unique_ptr<PackageParser> parser;
    std::unique_ptr<DeviceUpdater> deviceUpdater;
    uint8_t progressPercentage = 0;
    decltype(std::chrono::steady_clock::now()) startTime;

    /**
     * @brief The defer handler for processing package
     */
    std::unique_ptr<sdeventplus::source::Defer> deferHandler;

    friend class ItemBasedUpdate;
};

} // namespace pldm::fw_update
