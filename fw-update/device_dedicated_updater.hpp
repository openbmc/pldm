#pragma once

#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "device_updater.hpp"
#include "package_parser.hpp"
#include "requester/handler.hpp"
#include "software_update.hpp"
#include "update_manager.hpp"

#include <libpldm/base.h>

#include <xyz/openbmc_project/Association/Definitions/aserver.hpp>
#include <xyz/openbmc_project/Software/Activation/aserver.hpp>
#include <xyz/openbmc_project/Software/ActivationBlocksTransition/aserver.hpp>
#include <xyz/openbmc_project/Software/ActivationProgress/aserver.hpp>
#include <xyz/openbmc_project/Software/Version/aserver.hpp>

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

using SoftwareActivationProgress =
    sdbusplus::aserver::xyz::openbmc_project::software::ActivationProgress<
        DeviceDedicatedUpdater>;
using SoftwareActivationBlocksTransition =
    sdbusplus::aserver::xyz::openbmc_project::software::
        ActivationBlocksTransition<DeviceDedicatedUpdater>;
using SoftwareActivation =
    sdbusplus::aserver::xyz::openbmc_project::software::Activation<
        DeviceDedicatedUpdater>;
using SoftwareVersion =
    sdbusplus::aserver::xyz::openbmc_project::software::Version<
        DeviceDedicatedUpdater>;
using SoftwareAssociationDefinitions =
    sdbusplus::aserver::xyz::openbmc_project::association::Definitions<
        DeviceDedicatedUpdater>;
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
    DeviceDedicatedUpdater(DeviceDedicatedUpdater&&) = default;
    DeviceDedicatedUpdater& operator=(const DeviceDedicatedUpdater&) = delete;
    DeviceDedicatedUpdater& operator=(DeviceDedicatedUpdater&&) = delete;
    ~DeviceDedicatedUpdater() = default;

    explicit DeviceDedicatedUpdater(
        Context& ctx, Event& event,
        pldm::requester::Handler<pldm::requester::Request>& handler,
        InstanceIdDb& instanceIdDb, pldm::eid eid,
        const std::string& softwarePath, const std::string& softwareVersion,
        const std::string& associatedEndpoint, const Descriptors& descriptors,
        const ComponentInfo& componentInfo,
        SoftwareVersionPurpose purpose = SoftwareVersionPurpose::Unknown);

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
    sdbusplus::async::task<int> initUpdate(sdbusplus::message::unix_fd image,
                                           RequestedApplyTimes applyTime);

  private:
    int processPackageData();

    std::optional<DeviceUpdaterInfo> validatePackage();

    Context& ctx;

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

    const ComponentInfo& componentInfo;

    decltype(std::chrono::steady_clock::now()) startTime;
};

} // namespace fw_update

} // namespace pldm
