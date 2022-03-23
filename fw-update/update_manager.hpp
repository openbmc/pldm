#pragma once

#include "libpldm/base.h"
#include "libpldm/requester/pldm.h"

#include "common/types.hpp"
#include "device_updater.hpp"
#include "package_parser.hpp"
#include "pldmd/dbus_impl_requester.hpp"
#include "requester/handler.hpp"
#include "watch.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <tuple>
#include <unordered_map>

namespace pldm
{

namespace fw_update
{

using namespace sdeventplus;
using namespace sdeventplus::source;
using namespace pldm::dbus_api;
using namespace pldm;

using DeviceIDRecordOffset = size_t;
using DeviceUpdaterInfo = std::pair<mctp_eid_t, DeviceIDRecordOffset>;
using DeviceUpdaterInfos = std::vector<DeviceUpdaterInfo>;
using TotalComponentUpdates = size_t;

class Activation;
class ActivationProgress;

class UpdateManager
{
  public:
    UpdateManager() = delete;
    UpdateManager(const UpdateManager&) = delete;
    UpdateManager(UpdateManager&&) = delete;
    UpdateManager& operator=(const UpdateManager&) = delete;
    UpdateManager& operator=(UpdateManager&&) = delete;
    ~UpdateManager() = default;

    explicit UpdateManager(
        Event& event,
        pldm::requester::Handler<pldm::requester::Request>& handler,
        Requester& requester, const DescriptorMap& descriptorMap,
        const ComponentInfoMap& componentInfoMap,
        ComponentNameMap& componentNameMap) :
        event(event),
        handler(handler), requester(requester), descriptorMap(descriptorMap),
        componentInfoMap(componentInfoMap), componentNameMap(componentNameMap),
        watch(event.get(),
              std::bind_front(&UpdateManager::processPackage, this))
    {}

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
    Response handleRequest(mctp_eid_t eid, uint8_t command,
                           const pldm_msg* request, size_t reqMsgLen);

    int processPackage(const std::filesystem::path& packageFilePath);

    void updateDeviceCompletion(mctp_eid_t eid, bool status);

    void updateActivationProgress();

    /** @brief Callback function that will be invoked when the
     *         RequestedActivation will be set to active in the Activation
     *         interface
     */
    void activatePackage();

    void clearActivationInfo();

    /** @brief
     *
     */
    DeviceUpdaterInfos
        associatePkgToDevices(const FirmwareDeviceIDRecords& fwDeviceIDRecords,
                              const DescriptorMap& descriptorMap,
                              TotalComponentUpdates& totalNumComponentUpdates);

    /** @brief Handle Request to Create Message Registries for various
     *         Firmware update events
     *
     *  @param[in] eid - Remote MCTP Endpoint ID
     *  @param[in] fwDeviceIDRecord - fwDeviceIDRecord from Bundle
     *  @param[in] compIndex - component Index
     *  @param[in] messageID - messageID string
     *  @param[in] resolution - resolution for the event(optional)
     *
     */
    void createMessageRegistry(mctp_eid_t eid,
                               const FirmwareDeviceIDRecord& fwDeviceIDRecord,
                               size_t compIndex, const std::string messageID,
                               const std::string& resolution = std::string(""));

    /** @brief Method to call system api to create Log Entries.
     *
     *  @param[in] messageID - messageID string
     *  @param[in] compName - Component Name string
     *  @param[in] compVersion - Firmware Version string
     *  @param[in] resolution - resolution string
     *
     */
    void createMsgRegistryLog(const std::string& messageID,
                              const std::string& compName,
                              const std::string& compVersion,
                              const std::string& resolution);

    const std::string swRootPath{"/xyz/openbmc_project/software/"};
    Event& event; //!< reference to PLDM daemon's main event loop
    /** @brief PLDM request handler */
    pldm::requester::Handler<pldm::requester::Request>& handler;
    Requester& requester; //!< reference to Requester object

    const std::string transferFailed{"Update.1.0.TransferFailed"};
    const std::string transferringToComponent{
        "Update.1.0.TransferringToComponent"};
    const std::string verificationFailed{"Update.1.0.VerificationFailed"};
    const std::string updateSuccessful{"Update.1.0.UpdateSuccessful"};
    const std::string awaitToActivate{"Update.1.0.AwaitToActivate"};
    const std::string applyFailed{"Update.1.0.ApplyFailed"};
    const std::string activateFailed{"Update.1.0.ActivateFailed"};
    const std::string targetDetermined{"Update.1.0.TargetDetermined"};

  private:
    /** @brief Device identifiers of the managed FDs */
    const DescriptorMap& descriptorMap;
    /** @brief Component information needed for the update of the managed FDs */
    const ComponentInfoMap& componentInfoMap;
    /** @brief Component information needed for the update of the managed FDs */
    const ComponentNameMap& componentNameMap;
    Watch watch;

    std::unique_ptr<Activation> activation;
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

    /** @brief FW update package can contain updates for multiple firmware
     *         devices and each device can have multiple components. Once
     *         each component is updated (Transfer completed, Verified and
     *         Applied) ActivationProgress is updated.
     */
    size_t compUpdateCompletedCount;
    decltype(std::chrono::steady_clock::now()) startTime;
};

} // namespace fw_update

} // namespace pldm
