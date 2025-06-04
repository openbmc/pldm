#include "device_dedicated_updater.hpp"

#include "common/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <string>

using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

DeviceDedicatedUpdater::DeviceDedicatedUpdater(
    Context& ctx, Event& event,
    pldm::requester::Handler<pldm::requester::Request>& handler,
    InstanceIdDb& instanceIdDb, pldm::eid eid, const std::string& softwarePath,
    const std::string& softwareVersion, const std::string& associatedEndpoint,
    const Descriptors& descriptors, const ComponentInfo& componentInfo,
    SoftwareVersionPurpose purpose) :
    UpdateManagerIntf(event, handler, instanceIdDb), ctx(ctx), eid(eid),
    softwarePath(softwarePath),
    activation(
        std::make_unique<SoftwareActivation>(ctx, this->softwarePath.c_str())),
    association(std::make_unique<SoftwareAssociationDefinitions>(
        ctx, this->softwarePath.c_str())),
    version(std::make_unique<SoftwareVersion>(ctx, this->softwarePath.c_str())),
    updateInterface(
        std::make_unique<SoftwareUpdate>(ctx, this->softwarePath, *this)),
    descriptors(descriptors), componentInfo(componentInfo)
{
    this->association->associations(
        {{"running", "ran_on", associatedEndpoint.c_str()}});
    this->activation->activation(SoftwareActivation::Activations::Active);
    this->activation->emit_added();
    this->version->version(softwareVersion.c_str());
    this->version->purpose(purpose);
    this->version->emit_added();
}

sdbusplus::async::task<int> DeviceDedicatedUpdater::initUpdate(
    sdbusplus::message::unix_fd image, RequestedApplyTimes /*applyTime*/)
{
    if (image.fd < 0)
    {
        error("Invalid image file descriptor for EID {EID}", "EID", eid);
        updateInvalidRoutine();
        co_return -1;
    }
    // read fd to std::optional<std::vector<uint8_t>> packageData
    packageData = std::make_optional<std::vector<uint8_t>>();
    packageData->resize(0);
    std::vector<uint8_t> buffer(4096);
    ssize_t bytesRead;
    while ((bytesRead = read(image.fd, buffer.data(), buffer.size())) > 0)
    {
        packageData->insert(packageData->end(), buffer.begin(),
                            buffer.begin() + bytesRead);
    }
    if (bytesRead < 0)
    {
        error("Failed to read image data for EID {EID}", "EID", eid);
        updateInvalidRoutine();
        co_return -1;
    }
    if (packageData->empty())
    {
        error("No data read from image file descriptor for EID {EID}", "EID",
              eid);
        updateInvalidRoutine();
        co_return -1;
    }
    auto rc = processPackageData();
    if (rc < 0)
    {
        error("Failed to process package data for EID {EID}", "EID", eid);
        updateInvalidRoutine();
        co_return rc;
    }
    if (!deviceUpdater)
    {
        error("No device updater created for EID {EID}", "EID", eid);
        updateInvalidRoutine();
        co_return -1;
    }
    activation->activation(SoftwareActivation::Activations::Ready);
    activationProgress = nullptr;
    activationProgress =
        std::make_unique<SoftwareActivationProgress>(ctx, softwarePath.c_str());
    activationProgress->progress(0);
    activationProgress->emit_added();
    blocksTransition = std::make_unique<SoftwareActivationBlocksTransition>(
        ctx, softwarePath.c_str());
    activation->activation(SoftwareActivation::Activations::Activating);
    deviceUpdater->startFwUpdateFlow();
    co_return 0;
}

void DeviceDedicatedUpdater::updateInvalidRoutine()
{
    activation->activation(SoftwareActivation::Activations::Failed);
    blocksTransition = nullptr;
    activationProgress = nullptr;
    isUpdateInProgress = false;
}

int DeviceDedicatedUpdater::processPackageData()
{
    if (!packageData.has_value())
    {
        error("Package data is not available for processing.");
        return -1;
    }
    if (packageData->empty())
    {
        return 0;
    }
    try
    {
        parser = std::make_unique<PackageParser>(*packageData);
    }
    catch (const InternalFailure&)
    {
        parser = nullptr;
        error("Failed to parse package data for EID {EID}", "EID", eid);
        return -1;
    }

    auto deviceIDRecordOffset = validatePackage();
    if (!deviceIDRecordOffset.has_value())
    {
        error("Invalid package data for EID {EID}", "EID", eid);
        return -1;
    }
    const auto& fwDeviceIDRecords = parser->getFwDeviceIDRecords();
    const auto& downstreamDeviceIDRecords =
        parser->getDownstreamDeviceIDRecords();
    const auto& compImageInfos = parser->getComponentImageInfos();
    auto [eid, isDownstream, offset] = *deviceIDRecordOffset;

    if (isDownstream)
    {
        deviceIDRecord = downstreamDeviceIDRecords[offset];
    }
    else
    {
        deviceIDRecord = fwDeviceIDRecords[offset];
    }

    deviceUpdater = std::make_unique<DeviceUpdater>(
        eid, deviceIDRecord, compImageInfos, componentInfo,
        MAXIMUM_TRANSFER_SIZE, this);

    return 0;
}

std::optional<DeviceUpdaterInfo> DeviceDedicatedUpdater::validatePackage()
{
    auto descriptorVec =
        std::vector<Descriptor>(descriptors.begin(), descriptors.end());
    std::sort(descriptorVec.begin(), descriptorVec.end());
    const auto& fwDeviceIDRecord = parser->getFwDeviceIDRecords();
    const auto& downstreamDeviceIDRecord =
        parser->getDownstreamDeviceIDRecords();
    for (size_t index = 0; index < fwDeviceIDRecord.size(); ++index)
    {
        const auto& deviceIDDescriptors =
            std::get<Descriptors>(fwDeviceIDRecord[index]);
        std::vector<Descriptor> deviceIDDescriptorsVec(
            deviceIDDescriptors.begin(), deviceIDDescriptors.end());
        std::sort(deviceIDDescriptorsVec.begin(), deviceIDDescriptorsVec.end());
        if (std::includes(descriptorVec.begin(), descriptorVec.end(),
                          deviceIDDescriptorsVec.begin(),
                          deviceIDDescriptorsVec.end()))
        {
            return DeviceUpdaterInfo{eid, false, index};
        }
    }
    for (size_t index = 0; index < downstreamDeviceIDRecord.size(); ++index)
    {
        const auto& downstreamDeviceIDDescriptors =
            std::get<Descriptors>(downstreamDeviceIDRecord[index]);
        std::vector<Descriptor> downstreamDeviceIDDescriptorsVec(
            downstreamDeviceIDDescriptors.begin(),
            downstreamDeviceIDDescriptors.end());
        std::sort(downstreamDeviceIDDescriptorsVec.begin(),
                  downstreamDeviceIDDescriptorsVec.end());
        if (std::includes(descriptorVec.begin(), descriptorVec.end(),
                          downstreamDeviceIDDescriptorsVec.begin(),
                          downstreamDeviceIDDescriptorsVec.end()))
        {
            return DeviceUpdaterInfo{eid, true, index};
        }
    }
    return std::nullopt;
}

void DeviceDedicatedUpdater::updateDeviceCompletion(mctp_eid_t /*eid*/,
                                                    bool status)
{
    if (activationProgress)
    {
        activationProgress->progress(
            status ? 100 : 0); // Set progress to 100% if successful, else 0%
    }
    if (status)
    {
        activation->activation(SoftwareActivation::Activations::Active);
        blocksTransition = nullptr;
        activationProgress = nullptr;
        isUpdateInProgress = false;
    }
    else
    {
        activation->activation(SoftwareActivation::Activations::Failed);
        error("Firmware update failed for EID {EID}", "EID", eid);
    }
}
void DeviceDedicatedUpdater::updateActivationProgress()
{
    if (activationProgress)
    {
        activationProgress->progress(deviceUpdater->getProgress());
    }
}

Response DeviceDedicatedUpdater::handleRequest(
    Command command, const pldm_msg* request, size_t reqMsgLen)
{
    auto response = Response{sizeof(pldm_msg_hdr), 0};
    if (deviceUpdater)
    {
        Response ret;
        switch (command)
        {
            case PLDM_REQUEST_FIRMWARE_DATA:
                ret = deviceUpdater->requestFwData(request, reqMsgLen);
                updateActivationProgress();
                return ret;
            case PLDM_TRANSFER_COMPLETE:
                ret = deviceUpdater->transferComplete(request, reqMsgLen);
                updateActivationProgress();
                return ret;
            case PLDM_VERIFY_COMPLETE:
                ret = deviceUpdater->verifyComplete(request, reqMsgLen);
                updateActivationProgress();
                return ret;
            case PLDM_APPLY_COMPLETE:
                ret = deviceUpdater->applyComplete(request, reqMsgLen);
                updateActivationProgress();
                return ret;
            default:
                auto ptr = new (response.data()) pldm_msg;
                auto rc = encode_cc_only_resp(
                    request->hdr.instance_id, request->hdr.type,
                    request->hdr.command, PLDM_ERROR_INVALID_DATA, ptr);
                assert(rc == PLDM_SUCCESS);
        }
    }
    else
    {
        auto ptr = new (response.data()) pldm_msg;
        auto rc = encode_cc_only_resp(request->hdr.instance_id,
                                      request->hdr.type, request->hdr.command,
                                      PLDM_FWUP_COMMAND_NOT_EXPECTED, ptr);
        assert(rc == PLDM_SUCCESS);
    }
    return response;
}

} // namespace pldm::fw_update
