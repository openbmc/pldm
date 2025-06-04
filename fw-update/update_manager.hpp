#pragma once

#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "device_updater.hpp"
#include "fw-update/activation.hpp"
#include "package_parser.hpp"
#include "requester/handler.hpp"
#include "watch.hpp"

#include <libpldm/base.h>

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
using namespace pldm;

using DeviceIDRecordOffset = size_t;
using DeviceUpdaterInfo = std::pair<mctp_eid_t, DeviceIDRecordOffset>;
using DeviceUpdaterInfos = std::vector<DeviceUpdaterInfo>;
using TotalComponentUpdates = size_t;

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
    virtual void clearActivationInfo() = 0;

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
    ~UpdateManager() = default;

    explicit UpdateManager(
        Event& event,
        pldm::requester::Handler<pldm::requester::Request>& handler,
        InstanceIdDb& instanceIdDb, const DescriptorMap& descriptorMap,
        const ComponentInfoMap& componentInfoMap) :
        UpdateManagerBase(event, handler, instanceIdDb),
        descriptorMap(descriptorMap), componentInfoMap(componentInfoMap),
        watch(event.get(),
              [this](std::string& packageFilePath) {
                  return this->processPackage(
                      std::filesystem::path(packageFilePath));
              }),
        totalNumComponentUpdates(0)
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
    virtual Response handleRequest(mctp_eid_t eid, uint8_t command,
                                   const pldm_msg* request, size_t reqMsgLen);

    int processPackage(const std::filesystem::path& packageFilePath);

    void updateDeviceCompletion(mctp_eid_t eid, bool status) override;

    void updateActivationProgress() override;

    /** @brief Callback function that will be invoked when the
     *         RequestedActivation will be set to active in the Activation
     *         interface
     */
    void activatePackage() override;

    void clearActivationInfo() override;

    /** @brief
     *
     */
    DeviceUpdaterInfos associatePkgToDevices(
        const FirmwareDeviceIDRecords& fwDeviceIDRecords,
        const DescriptorMap& descriptorMap,
        TotalComponentUpdates& totalNumComponentUpdates);

    const std::string swRootPath{"/xyz/openbmc_project/software/"};

  private:
    /** @brief Device identifiers of the managed FDs */
    const DescriptorMap& descriptorMap;
    /** @brief Component information needed for the update of the managed FDs */
    const ComponentInfoMap& componentInfoMap;
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

    decltype(std::chrono::steady_clock::now()) startTime;
};

} // namespace fw_update

} // namespace pldm
