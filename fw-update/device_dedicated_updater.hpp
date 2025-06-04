#pragma once

#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "condition_executor.hpp"
#include "device_updater.hpp"
#include "package_parser.hpp"
#include "requester/handler.hpp"
#include "software_update.hpp"
#include "update_manager.hpp"

#include <libpldm/base.h>

#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/ActivationBlocksTransition/server.hpp>
#include <xyz/openbmc_project/Software/ActivationProgress/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <tuple>
#include <unordered_map>

namespace pldm
{

namespace fw_update
{

class DeviceDedicatedUpdater;
class SoftwareUpdate;

using SoftwareActivationProgress = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::ActivationProgress>;
using SoftwareActivationBlocksTransition =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::Software::
                                    server::ActivationBlocksTransition>;
using SoftwareActivation = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::Activation>;
using SoftwareVersion = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::Version>;
using SoftwareAssociationDefinitions = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

using SoftwareVersionPurpose = SoftwareVersion::VersionPurpose;

using namespace sdeventplus;
using namespace sdeventplus::source;
using namespace pldm;
using Context = sdbusplus::async::context;

class DeviceDedicatedUpdater : public UpdateManagerIntf
{
  public:
    DeviceDedicatedUpdater() = delete;
    DeviceDedicatedUpdater(const DeviceDedicatedUpdater&) = delete;
    DeviceDedicatedUpdater(DeviceDedicatedUpdater&&) = delete;
    DeviceDedicatedUpdater& operator=(const DeviceDedicatedUpdater&) = delete;
    DeviceDedicatedUpdater& operator=(DeviceDedicatedUpdater&&) = delete;
    ~DeviceDedicatedUpdater() = default;

    explicit DeviceDedicatedUpdater(
        Event& event,
        pldm::requester::Handler<pldm::requester::Request>& handler,
        InstanceIdDb& instanceIdDb, pldm::eid eid,
        const std::string& softwarePath, const std::string& softwareVersion,
        const std::string& associatedEndpoint, const Descriptors& descriptors,
        const ComponentInfo& componentInfo,
        SoftwareVersionPurpose purpose = SoftwareVersionPurpose::Unknown,
        const ConditionPaths& conditionPathPair = ConditionPaths{},
        const std::string& conditionArg = std::string{},
        std::function<void()> taskCompletionCallback = nullptr);

    /** @brief Handle PLDM request for the commands in the FW update
     *         specification
     *
     *  @param[in] command - PLDM command code
     *  @param[in] request - PLDM request message
     *  @param[in] requestLen - PLDM request message length
     *
     *  @return PLDM response message
     */
    Response handleRequest(uint8_t command, const pldm_msg* request,
                           size_t reqMsgLen);

    void updateActivationProgress() override;
    void updateDeviceCompletion(mctp_eid_t eid, bool status) override;
    void sendInitUpdateEvent(
        sdbusplus::message::unix_fd image,
        RequestedApplyTimes applyTime = RequestedApplyTimes::Immediate);

  private:
    void initUpdate(sdbusplus::message::unix_fd image,
                    RequestedApplyTimes applyTime);
    int processPackageData();

    void handleUpdateFailure();

    std::optional<DeviceUpdaterInfo> validatePackage();

    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();

    Event& event;

    pldm::eid eid;

    std::string softwarePath;

    DeviceIDRecord deviceIDRecord;

    std::optional<std::vector<uint8_t>> packageData;

    std::unique_ptr<PackageParser> parser;

    std::unique_ptr<DeviceUpdater> deviceUpdater;

    std::unique_ptr<SoftwareActivation> activation;

    std::unique_ptr<SoftwareActivationProgress> activationProgress;

    std::unique_ptr<SoftwareActivationBlocksTransition> blocksTransition;

    std::unique_ptr<SoftwareAssociationDefinitions> association;

    std::unique_ptr<SoftwareVersion> version;

    std::unique_ptr<SoftwareUpdate> updateInterface;

    const Descriptors& descriptors;

    ComponentInfo componentInfo;

    bool isUpdateInProgress = false;

    std::unique_ptr<sdbusplus::Timer> initUpdateEvent;

    const std::chrono::seconds initUpdateInterval = std::chrono::seconds(1);

    std::string preConditionPath;
    std::string postConditionPath;
    std::string conditionArg;

    std::function<void()> taskCompletionCallback;

    friend class SoftwareUpdate;
};

} // namespace fw_update

} // namespace pldm
