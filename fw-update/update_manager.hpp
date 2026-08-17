#pragma once
#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "condition_collector.hpp"
#include "device_updater.hpp"
#include "fw-update/activation.hpp"
#include "fw-update/update.hpp"

#ifdef FW_UPDATE_INOTIFY_ENABLED
#include "fw-update/watch.hpp"
#endif
#include "package_parser.hpp"
#include "requester/handler.hpp"

#include <libpldm/base.h>

#include <sdbusplus/async.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <unordered_map>
#include <utility>

namespace pldm
{

namespace fw_update
{

using namespace sdeventplus;
using namespace sdeventplus::source;
using namespace pldm;

using DeviceIDRecordOffset = size_t;
using DeviceUpdaterInfo = std::pair<mctp_eid_t, DeviceIDRecordOffset>;
using DeviceUpdaterInfos = std::vector<DeviceUpdaterInfo>;
using TotalComponentUpdates = size_t;

/**
 * @brief Sensor-polling action requested around a device's firmware update
 */
enum class SensorPollingAction
{
    Stop,
    Resume,
};

/**
 * @brief The base class of the UpdateManager and the
 *        ItemBaseUpdateManager
 */
class UpdateManagerBase
{
  public:
    virtual ~UpdateManagerBase() = default;

    UpdateManagerBase() = delete;
    UpdateManagerBase(const UpdateManagerBase&) = delete;
    UpdateManagerBase(UpdateManagerBase&&) = delete;
    UpdateManagerBase& operator=(const UpdateManagerBase&) = delete;
    UpdateManagerBase& operator=(UpdateManagerBase&&) = delete;
    /** @brief Constructor
     *
     *  @param[in] event - PLDM daemon's main event loop
     *  @param[in] handler - PLDM request handler
     *  @param[in] instanceIdDb - Managing instance ID for PLDM requests
     */
    UpdateManagerBase(
        Event& event,
        pldm::requester::Handler<pldm::requester::Request>& handler,
        InstanceIdDb& instanceIdDb) :
        event(event), handler(handler), instanceIdDb(instanceIdDb)
    {}

    virtual void updateDeviceCompletion(mctp_eid_t eid, bool status) = 0;
    virtual void updateActivationProgress() = 0;
    virtual void activatePackage() = 0;
    virtual void resetActivationState() = 0;

    Event& event;               //!< reference to PLDM daemon's main event loop
    pldm::requester::Handler<pldm::requester::Request>& handler;
    InstanceIdDb& instanceIdDb; //!< reference to an InstanceIdDb
};

class UpdateManager : public UpdateManagerBase
{
  public:
    UpdateManager() = delete;
    UpdateManager(const UpdateManager&) = delete;
    UpdateManager(UpdateManager&&) = delete;
    UpdateManager& operator=(const UpdateManager&) = delete;
    UpdateManager& operator=(UpdateManager&&) = delete;
    virtual ~UpdateManager() = default;

    explicit UpdateManager(
        Event& event,
        pldm::requester::Handler<pldm::requester::Request>& handler,
        InstanceIdDb& instanceIdDb, const DescriptorMap& descriptorMap,
        const ComponentInfoMap& componentInfoMap) :
        UpdateManagerBase(event, handler, instanceIdDb),
        descriptorMap(descriptorMap), componentInfoMap(componentInfoMap),
#ifdef FW_UPDATE_INOTIFY_ENABLED
        watch(event.get(),
              [this](const std::string& packageFilePath) {
                  return this->processPackage(
                      std::filesystem::path(packageFilePath));
              }),
#else
        updater(std::make_unique<Update>(pldm::utils::DBusHandler::getBus(),
                                         "/xyz/openbmc_project/software/pldm",
                                         this)),
#endif
        totalNumComponentUpdates(0)
    {}

    /**
     * @brief Set the callback used to pause/resume sensor polling for a
     * device's terminus around its firmware update
     *
     * @param[in] sensorPollingCallback Callback invoked with the terminus TID
     * and the requested action
     */
    void setSensorPollingCallback(
        std::function<void(mctp_eid_t, SensorPollingAction)>
            sensorPollingCallback)
    {
        this->sensorPollingCallback = std::move(sensorPollingCallback);
    }

    /** @brief Handle PLDM request for the commands in the FW update
     *         specification
     *
     *  @param[in] eid - Remote MCTP Endpoint ID
     *  @param[in] command - PLDM command code
     *  @param[in] request - PLDM request message
     *  @param[in] requestLen - PLDM request message length
     *
     *  @return PLDM response message
     */
    virtual Response handleRequest(mctp_eid_t eid, uint8_t command,
                                   const pldm_msg* request, size_t reqMsgLen);

    int processPackage(const std::filesystem::path& packageFilePath);

    /** @brief Process the firmware update package
     *
     *  @param[in] packageStream - Stream of the firmware update package
     *  @param[in] packageSize - Size of the firmware update package
     *
     *  @return Object path of the created Software object
     */
    void processStream(std::istream& packageStream, uintmax_t packageSize);

    /** @brief Defers processing the package stream
     *
     *  @param[in] packageStream - Stream of the firmware update package
     *  @param[in] packageSize - Size of the firmware update package
     *
     *  @return Object path of the created Software object as a string
     */
    std::string processStreamDefer(std::istream& packageStream,
                                   uintmax_t packageSize);

    void updateDeviceCompletion(mctp_eid_t eid, bool status) override;

    void updateActivationProgress() override;

    /** @brief Callback function that will be invoked when the
     *         RequestedActivation will be set to active in the Activation
     *         interface
     */
    void activatePackage() override;

    void resetActivationState() override;

    /** @brief
     *
     */
    DeviceUpdaterInfos associatePkgToDevices(
        const FirmwareDeviceIDRecords& fwDeviceIDRecords,
        const DescriptorMap& descriptorMap,
        TotalComponentUpdates& totalNumComponentUpdates);

    /** @brief Generate a unique software ID based on current timestamp
     *
     *  @return String representation of the current timestamp in seconds
     */
    static std::string getSwId();

    const std::string swRootPath{"/xyz/openbmc_project/software/"};

    std::unique_ptr<Activation> activation;

  private:
    /** @brief Starts firmware activation for all associated devices.
     */
    void startFirmwareUpdate();

    /** @brief Completes the update flow and sets final activation state.
     *
     *  @param[in] status - Overall update status, true on success
     */
    void completeUpdate(bool status);

    /** @brief Device identifiers of the managed FDs */
    const DescriptorMap& descriptorMap;
    /** @brief Component information needed for the update of the managed FDs */
    const ComponentInfoMap& componentInfoMap;
#ifdef FW_UPDATE_INOTIFY_ENABLED
    Watch watch;
#else
    std::unique_ptr<Update> updater;
#endif

    std::unique_ptr<ActivationProgress> activationProgress;
    std::string objPath;

    std::filesystem::path fwPackageFilePath;
    std::unique_ptr<PackageParser> parser;
    std::ifstream package;

    std::unordered_map<mctp_eid_t, std::unique_ptr<DeviceUpdater>>
        deviceUpdaterMap;
    std::unordered_map<mctp_eid_t, bool> deviceUpdateCompletionMap;

    /** @brief Total number of component updates to calculate the progress of
     *         the Firmware activation
     */
    size_t totalNumComponentUpdates;

    decltype(std::chrono::steady_clock::now()) startTime;
    std::unique_ptr<sdeventplus::source::Defer> updateDeferHandler;

    std::string preConditionPath;
    std::string postConditionPath;
    std::string conditionArg;

    /**
     * @brief Whether this package's condition config sets
     * 'StopSensorPolling' to true, applied to every device in the package
     */
    bool stopSensorPollingDuringUpdate = false;

    std::function<void()> taskCompletionCallback;

  protected:
    /**
     * @brief Callback to pause/resume sensor polling for a device's
     * terminus. A no-op when unset.
     */
    std::function<void(mctp_eid_t, SensorPollingAction)> sensorPollingCallback;

  private:
    bool updateInProgress = false;

    /** @brief The last progress that was calculated. Used to avoid spamming
     * dbus
     *
     */
    uint8_t lastProgress;
};

} // namespace fw_update

} // namespace pldm
