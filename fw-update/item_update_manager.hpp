#include "update_manager.hpp"

#include <xyz/openbmc_project/Software/ApplyTime/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

#include <span>
#include <spanstream>

namespace pldm::fw_update
{

using namespace sdeventplus;
using namespace sdeventplus::source;
using namespace pldm;

namespace software = sdbusplus::xyz::openbmc_project::Software::server;

using ItemUpdateIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::Update>;
using ApplyTimeIntf =
    sdbusplus::xyz::openbmc_project::Software::server::ApplyTime;

class ItemUpdateManager : public UpdateManagerBase, public ItemUpdateIntf
{
  public:
    ItemUpdateManager() = delete;
    ItemUpdateManager(const ItemUpdateManager&) = delete;
    ItemUpdateManager(ItemUpdateManager&&) = delete;
    ItemUpdateManager& operator=(const ItemUpdateManager&) = delete;
    ItemUpdateManager& operator=(ItemUpdateManager&&) = delete;
    ~ItemUpdateManager() = default;

    /**
     * @brief Constructor for ItemUpdateManager
     *
     * @param[in] eid The MCTP EID
     * @param[in] event The event object
     * @param[in] handler The request handler
     * @param[in] instanceIdDb The instance ID database
     * @param[in] objPath The D-Bus object path
     * @param[in] generatedId The software hash identifier
     * @param[in] descriptors The descriptors for the device
     * @param[in] componentInfo The component information for the device
     */
    explicit ItemUpdateManager(
        mctp_eid_t eid, Event& event,
        pldm::requester::Handler<pldm::requester::Request>& handler,
        InstanceIdDb& instanceIdDb, const std::string& objPath,
        const std::string& generatedId, const Descriptors& descriptors,
        const ComponentInfo& componentInfo) :
        UpdateManagerBase(event, handler, instanceIdDb),
        ItemUpdateIntf(pldm::utils::DBusHandler::getBus(),
                       std::format("{}_{}", objPath, generatedId).c_str()),
        eid(eid), objPath(objPath), descriptors(descriptors),
        componentInfo(componentInfo)
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
    void resetActivationState() override;

    /**
     * @brief Start the update process
     *
     * D-Bus method implementation for starting the update process
     *
     * @param[in] image The image file descriptor
     * @param[in] applyTime The requested apply time
     */
    virtual sdbusplus::message::object_path startUpdate(
        sdbusplus::message::unix_fd image,
        ApplyTimeIntf::RequestedApplyTimes applyTime =
            ApplyTimeIntf::RequestedApplyTimes::Immediate) override;

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
    std::string objPathWithSwId;

    /**
     * @brief The descriptors to match against
     */
    const Descriptors& descriptors;

    /**
     * @brief The component information of the target device
     */
    const ComponentInfo& componentInfo;

    /**
     * @brief The package data for the firmware update
     */
    std::unique_ptr<pldm::utils::MMapHandler> packageMap;

    /**
     * @brief The package data stream for the firmware update
     */
    std::unique_ptr<std::ispanstream> packageDataStream;

    /**
     * @brief Process the firmware update package
     *
     * @return true on success, false on failure
     */
    bool processPackage();

    /**
     * @brief Send the defer request of the firmware update package
     *
     * @param[in] fd - The firmware update package file descriptor
     * @return The D-Bus object path of the firmware update package
     */
    std::string processFd(int fd);

    std::unique_ptr<Activation> activation;
    std::unique_ptr<ActivationProgress> activationProgress;
    std::unique_ptr<PackageParser> parser;
    std::unique_ptr<DeviceUpdater> deviceUpdater;
    decltype(std::chrono::steady_clock::now()) startTime;

    /**
     * @brief The defer handler for processing package
     */
    std::unique_ptr<sdeventplus::source::Defer> deferHandler;

    bool updateInProgress = false;
};

} // namespace pldm::fw_update
