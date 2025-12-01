#include "device_updater.hpp"

#include "activation.hpp"
#include "update_manager.hpp"

#include <libpldm/firmware_update.h>

#include <phosphor-logging/lg2.hpp>

#include <functional>

using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

PHOSPHOR_LOG2_USING;

namespace pldm
{

namespace fw_update
{

// The highest percentage that component transfer can give
static constexpr uint8_t componentXferProgressPercent = 97;
static constexpr uint8_t componentVerifyProgressPercent = 98;
static constexpr uint8_t componentApplyProgressPercent = 99;
static constexpr uint8_t firmwareActivationProgressPercent = 100;

UpdateProgress::UpdateProgress(uint32_t totalSize, mctp_eid_t eid) :
    progress{}, eid{eid}, totalSize{totalSize}, totalUpdated{},
    currentState{state::Update}
{}

uint32_t UpdateProgress::getTotalSize() const
{
    return totalSize;
}
uint8_t UpdateProgress::getProgress() const
{
    return progress;
}

void UpdateProgress::updateState(state newState)
{
    switch (newState)
    {
        case state::Update:
            break;
        case state::Verify:
            progress = componentVerifyProgressPercent;
            break;
        case state::Apply:
            progress = componentApplyProgressPercent;
            break;
        default:
            warning("Invalid state {STATE} provided", "STATE", newState);
            return;
    };
    currentState = newState;
}

void UpdateProgress::reportFwUpdate(uint32_t amountUpdated)
{
    if (currentState != state::Update)
    {
        error("invalid state when updating progress: eid {EID}", "EID", eid);
        return;
    }

    if (amountUpdated > totalSize)
    {
        error("invalid size when updating progress: eid {EID}", "EID", eid);
        return;
    }

    if (amountUpdated + totalUpdated > totalSize)
    {
        error("invalid total size when updating progress: eid {EID}", "EID",
              eid);
        return;
    }

    if (totalSize == 0)
    {
        error("invalid package size when updating progress: eid {EID}", "EID",
              eid);
        return;
    }

    totalUpdated += amountUpdated;
    progress = static_cast<uint8_t>(std::floor(
        1.0 * componentXferProgressPercent * totalUpdated / totalSize));
    return;
}

DeviceUpdater::DeviceUpdater(
    mctp_eid_t eid, const DeviceIDRecord& deviceIDRecord,
    const ComponentImageInfos& compImageInfos, const ComponentInfo& compInfo,
    uint32_t maxTransferSize, UpdateManager* updateManager) :
    eid(eid), compImageInfos(compImageInfos), compInfo(compInfo),
    maxTransferSize(maxTransferSize), updateManager(updateManager),
    activationComplete{false}
{
    // Get the applicable components from the device ID record
    if (auto* firmwareRecord =
            std::get_if<FirmwareDeviceIDRecord>(&deviceIDRecord))
    {
        applicableComponents = std::get<ApplicableComponents>(*firmwareRecord);
        firmwareDevicePackageData =
            std::get<FirmwareDevicePackageData>(*firmwareRecord);
        componentImageSetVersion =
            std::get<ComponentImageSetVersion>(*firmwareRecord);
    }
    else if (auto* downstreamRecord =
                 std::get_if<DownstreamDeviceIDRecord>(&deviceIDRecord))
    {
        applicableComponents =
            std::get<ApplicableComponents>(*downstreamRecord);
        firmwareDevicePackageData =
            std::get<FirmwareDevicePackageData>(*downstreamRecord);
        // Note: This is only a workaround before we add a dedicated
        // command handler for downstream devices.
        componentImageSetVersion = "";
    }
    else
    {
        error("Invalid device ID record type for endpoint ID '{EID}'", "EID",
              eid);
        throw InternalFailure();
    }

    // create as many progress objects as there are components
    // and initialize them with the size of each component
    for (const auto& applicableComponent : applicableComponents)
    {
        const auto& componentTuple = compImageInfos[applicableComponent];
        const auto& componentImage = std::get<CompImage>(componentTuple);
        progress.emplace_back(static_cast<uint32_t>(componentImage.size()),
                              eid);
    }
}

void DeviceUpdater::startFwUpdateFlow()
{
    auto instanceIdResult = updateManager->instanceIdDb.next(eid);
    if (!instanceIdResult)
    {
        throw pldm::InstanceIdError(instanceIdResult.error());
    }
    auto instanceId = instanceIdResult.value();
    variable_field compImgSetVerStrInfo{};
    compImgSetVerStrInfo.ptr =
        reinterpret_cast<const uint8_t*>(componentImageSetVersion.data());
    compImgSetVerStrInfo.length =
        static_cast<uint8_t>(componentImageSetVersion.size());

    Request request(
        sizeof(pldm_msg_hdr) + sizeof(struct pldm_request_update_req) +
        compImgSetVerStrInfo.length);
    auto requestMsg = new (request.data()) pldm_msg;

    auto rc = encode_request_update_req(
        instanceId, maxTransferSize, applicableComponents.size(),
        PLDM_FWUP_MIN_OUTSTANDING_REQ, firmwareDevicePackageData.size(),
        PLDM_STR_TYPE_ASCII, compImgSetVerStrInfo.length, &compImgSetVerStrInfo,
        requestMsg,
        sizeof(struct pldm_request_update_req) + compImgSetVerStrInfo.length);
    if (rc)
    {
        // Handle error scenario
        updateManager->instanceIdDb.free(eid, instanceId);
        error(
            "Failed to encode request update request for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
    }

    rc = updateManager->handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_REQUEST_UPDATE, std::move(request),
        [this](mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen) {
            this->requestUpdate(eid, response, respMsgLen);
        });
    if (rc)
    {
        // Handle error scenario
        error(
            "Failed to send request update for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
    }
}

void DeviceUpdater::requestUpdate(mctp_eid_t eid, const pldm_msg* response,
                                  size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        // Handle error scenario
        error("No response received for request update for endpoint ID '{EID}'",
              "EID", eid);
        updateManager->updateDeviceCompletion(eid, false);
        return;
    }

    uint8_t completionCode = 0;
    uint16_t fdMetaDataLen = 0;
    uint8_t fdWillSendPkgData = 0;

    auto rc = decode_request_update_resp(response, respMsgLen, &completionCode,
                                         &fdMetaDataLen, &fdWillSendPkgData);
    if (rc)
    {
        error(
            "Failed to decode request update response for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
        updateManager->updateDeviceCompletion(eid, false);
        return;
    }
    if (completionCode)
    {
        error(
            "Failure in request update response for endpoint ID '{EID}', completion code '{CC}'",
            "EID", eid, "CC", completionCode);
        updateManager->updateDeviceCompletion(eid, false);
        return;
    }

    // Optional fields DeviceMetaData and GetPackageData not handled
    pldmRequest = std::make_unique<sdeventplus::source::Defer>(
        updateManager->event,
        std::bind(&DeviceUpdater::sendPassCompTableRequest, this,
                  componentIndex));
}

void DeviceUpdater::sendPassCompTableRequest(size_t offset)
{
    pldmRequest.reset();

    auto instanceIdResult = updateManager->instanceIdDb.next(eid);
    if (!instanceIdResult)
    {
        throw pldm::InstanceIdError(instanceIdResult.error());
    }
    auto instanceId = instanceIdResult.value();
    uint8_t transferFlag = 0;

    if (applicableComponents.size() == 1)
    {
        transferFlag = PLDM_START_AND_END;
    }
    else if (offset == 0)
    {
        transferFlag = PLDM_START;
    }
    else if (offset == applicableComponents.size() - 1)
    {
        transferFlag = PLDM_END;
    }
    else
    {
        transferFlag = PLDM_MIDDLE;
    }
    const auto& componentTuple = compImageInfos[applicableComponents[offset]];
    const auto& componentInfo =
        std::get<pldm_package_component_image_information>(componentTuple);
    CompClassification compClassification =
        componentInfo.component_classification;
    CompIdentifier compIdentifier = componentInfo.component_identifier;
    // ComponentClassificationIndex
    CompClassificationIndex compClassificationIndex{};
    auto compKey = std::make_pair(compClassification, compIdentifier);
    if (compInfo.contains(compKey))
    {
        auto search = compInfo.find(compKey);
        compClassificationIndex = search->second;
    }
    else
    {
        // Handle error scenario
        error(
            "Failed to find component classification '{CLASSIFICATION}' and identifier '{IDENTIFIER}'",
            "CLASSIFICATION", compClassification, "IDENTIFIER", compIdentifier);
    }
    CompComparisonStamp compComparisonStamp =
        componentInfo.component_comparison_stamp;
    const auto& componentVersion = std::get<CompVersion>(componentTuple);
    variable_field compVerStrInfo{};
    compVerStrInfo.ptr =
        reinterpret_cast<const uint8_t*>(componentVersion.data());
    compVerStrInfo.length = static_cast<uint8_t>(componentVersion.size());

    Request request(
        sizeof(pldm_msg_hdr) + sizeof(struct pldm_pass_component_table_req) +
        compVerStrInfo.length);
    auto requestMsg = new (request.data()) pldm_msg;
    auto rc = encode_pass_component_table_req(
        instanceId, transferFlag, compClassification, compIdentifier,
        compClassificationIndex, compComparisonStamp, PLDM_STR_TYPE_ASCII,
        compVerStrInfo.length, &compVerStrInfo, requestMsg,
        sizeof(pldm_pass_component_table_req) + compVerStrInfo.length);
    if (rc)
    {
        // Handle error scenario
        updateManager->instanceIdDb.free(eid, instanceId);
        error(
            "Failed to encode pass component table req for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
    }

    rc = updateManager->handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_PASS_COMPONENT_TABLE,
        std::move(request),
        [this](mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen) {
            this->passCompTable(eid, response, respMsgLen);
        });
    if (rc)
    {
        // Handle error scenario
        error(
            "Failed to send pass component table request for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
    }
}

void DeviceUpdater::passCompTable(mctp_eid_t eid, const pldm_msg* response,
                                  size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        // Handle error scenario
        error(
            "No response received for pass component table for endpoint ID '{EID}'",
            "EID", eid);
        updateManager->updateDeviceCompletion(eid, false);
        return;
    }

    uint8_t completionCode = 0;
    uint8_t compResponse = 0;
    uint8_t compResponseCode = 0;

    auto rc =
        decode_pass_component_table_resp(response, respMsgLen, &completionCode,
                                         &compResponse, &compResponseCode);
    if (rc)
    {
        // Handle error scenario
        error(
            "Failed to decode pass component table response for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
        updateManager->updateDeviceCompletion(eid, false);
        return;
    }
    if (completionCode)
    {
        // Handle error scenario
        error(
            "Failed to pass component table response for endpoint ID '{EID}', completion code '{CC}'",
            "EID", eid, "CC", completionCode);
        updateManager->updateDeviceCompletion(eid, false);
        return;
    }
    // Handle ComponentResponseCode

    if (componentIndex == applicableComponents.size() - 1)
    {
        componentIndex = 0;
        pldmRequest = std::make_unique<sdeventplus::source::Defer>(
            updateManager->event,
            std::bind(&DeviceUpdater::sendUpdateComponentRequest, this,
                      componentIndex));
    }
    else
    {
        componentIndex++;
        pldmRequest = std::make_unique<sdeventplus::source::Defer>(
            updateManager->event,
            std::bind(&DeviceUpdater::sendPassCompTableRequest, this,
                      componentIndex));
    }
}

void DeviceUpdater::sendUpdateComponentRequest(size_t offset)
{
    pldmRequest.reset();
    auto instanceIdResult = updateManager->instanceIdDb.next(eid);
    if (!instanceIdResult)
    {
        throw pldm::InstanceIdError(instanceIdResult.error());
    }
    auto instanceId = instanceIdResult.value();

    const auto& componentTuple = compImageInfos[applicableComponents[offset]];
    const auto& componentInfo =
        std::get<pldm_package_component_image_information>(componentTuple);
    CompClassification compClassification =
        componentInfo.component_classification;
    CompIdentifier compIdentifier = componentInfo.component_identifier;
    const auto& componentImage = std::get<CompImage>(componentTuple);
    auto compImageSize = componentImage.size();
    // ComponentClassificationIndex
    CompClassificationIndex compClassificationIndex{};
    auto compKey = std::make_pair(compClassification, compIdentifier);
    if (compInfo.contains(compKey))
    {
        auto search = compInfo.find(compKey);
        compClassificationIndex = search->second;
    }
    else
    {
        // Handle error scenario
        error(
            "Failed to find component classification '{CLASSIFICATION}' and identifier '{IDENTIFIER}'",
            "CLASSIFICATION", compClassification, "IDENTIFIER", compIdentifier);
    }

    bitfield32_t updateOptionFlags;
    updateOptionFlags.bits.bit0 = componentInfo.component_options.value;
    const auto& componentVersion = std::get<CompVersion>(componentTuple);
    variable_field compVerStrInfo{};
    compVerStrInfo.ptr =
        reinterpret_cast<const uint8_t*>(componentVersion.data());
    compVerStrInfo.length = static_cast<uint8_t>(componentVersion.size());

    Request request(
        sizeof(pldm_msg_hdr) + sizeof(struct pldm_update_component_req) +
        compVerStrInfo.length);
    auto requestMsg = new (request.data()) pldm_msg;

    auto rc = encode_update_component_req(
        instanceId, compClassification, compIdentifier, compClassificationIndex,
        componentInfo.component_comparison_stamp, compImageSize,
        updateOptionFlags, PLDM_STR_TYPE_ASCII, compVerStrInfo.length,
        &compVerStrInfo, requestMsg,
        sizeof(pldm_update_component_req) + compVerStrInfo.length);
    if (rc)
    {
        // Handle error scenario
        updateManager->instanceIdDb.free(eid, instanceId);
        error(
            "Failed to encode update component req for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
    }

    rc = updateManager->handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_UPDATE_COMPONENT, std::move(request),
        [this](mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen) {
            this->updateComponent(eid, response, respMsgLen);
        });
    if (rc)
    {
        // Handle error scenario
        error(
            "Failed to send update request for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
    }
}

void DeviceUpdater::updateComponent(mctp_eid_t eid, const pldm_msg* response,
                                    size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        // Handle error scenario
        error(
            "No response received for update component with endpoint ID {EID}",
            "EID", eid);
        updateManager->updateDeviceCompletion(eid, false);
        return;
    }

    uint8_t completionCode = 0;
    uint8_t compCompatibilityResp = 0;
    uint8_t compCompatibilityRespCode = 0;
    bitfield32_t updateOptionFlagsEnabled{};
    uint16_t timeBeforeReqFWData = 0;

    auto rc = decode_update_component_resp(
        response, respMsgLen, &completionCode, &compCompatibilityResp,
        &compCompatibilityRespCode, &updateOptionFlagsEnabled,
        &timeBeforeReqFWData);
    if (rc)
    {
        error(
            "Failed to decode update request response for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
        updateManager->updateDeviceCompletion(eid, false);
        return;
    }
    if (completionCode)
    {
        error(
            "Failed to update request response for endpoint ID '{EID}', completion code '{CC}'",
            "EID", eid, "CC", completionCode);
        updateManager->updateDeviceCompletion(eid, false);
        return;
    }

    const auto& applicableComponents =
        std::get<ApplicableComponents>(fwDeviceIDRecord);
    if (applicableComponents.empty())
    {
        error("No applicable components for endpoint ID '{EID}'", "EID", eid);
        updateManager->updateDeviceCompletion(eid, false);
        return;
    }
    const auto& comp = compImageInfos[applicableComponents[componentIndex]];
    const auto& compVersion =
        std::get<static_cast<size_t>(ComponentImageInfoPos::CompVersionPos)>(
            comp);

    if (compCompatibilityResp != PLDM_CCR_COMP_CAN_BE_UPDATED &&
        compCompatibilityResp != PLDM_CCR_COMP_CANNOT_BE_UPDATED)
    {
        error("Unknown compCompatibilityResp '{RESP}' for endpoint ID '{EID}'",
              "RESP", compCompatibilityResp, "EID", eid);
        updateManager->updateDeviceCompletion(eid, false);
        return;
    }

    if (compCompatibilityResp == PLDM_CCR_COMP_CANNOT_BE_UPDATED)
    {
        info(
            "Component at endpoint ID '{EID}' with version '{COMPONENT_VERSION}' cannot be updated, response code '{RESP_CODE}', skipping",
            "EID", eid, "COMPONENT_VERSION", compVersion, "RESP_CODE",
            compCompatibilityRespCode);
        componentUpdateStatus[componentIndex] = false;

        if (componentIndex == applicableComponents.size() - 1)
        {
            for (const auto& compStatus : componentUpdateStatus)
            {
                if (compStatus.second)
                {
                    componentIndex = 0;
                    pldmRequest = std::make_unique<sdeventplus::source::Defer>(
                        updateManager->event,
                        std::bind(&DeviceUpdater::sendActivateFirmwareRequest,
                                  this));
                    return;
                }
            }
            updateManager->updateDeviceCompletion(eid, false);
        }
        else
        {
            componentIndex++;
            pldmRequest = std::make_unique<sdeventplus::source::Defer>(
                updateManager->event,
                std::bind(&DeviceUpdater::sendUpdateComponentRequest, this,
                          componentIndex));
        }
        return;
    }

    info(
        "Component at endpoint ID '{EID}' with version '{COMPONENT_VERSION}' can be updated",
        "EID", eid, "COMPONENT_VERSION", compVersion);
    componentUpdateStatus[componentIndex] = true;
    createRequestFwDataTimer();
}

void DeviceUpdater::createRequestFwDataTimer()
{
    reqFwDataTimer = std::make_unique<sdbusplus::Timer>([this]() -> void {
        componentUpdateStatus[componentIndex] = false;
        sendCancelUpdateComponentRequest();
        updateManager->updateDeviceCompletion(eid, false);
    });
}

Response DeviceUpdater::requestFwData(const pldm_msg* request,
                                      size_t payloadLength)
{
    uint8_t completionCode = PLDM_SUCCESS;
    uint32_t offset = 0;
    uint32_t length = 0;
    Response response(sizeof(pldm_msg_hdr) + sizeof(completionCode), 0);
    auto responseMsg = new (response.data()) pldm_msg;
    auto rc = decode_request_firmware_data_req(request, payloadLength, &offset,
                                               &length);
    if (rc)
    {
        error(
            "Failed to decode request firmware date request for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
        rc = encode_request_firmware_data_resp(
            request->hdr.instance_id, PLDM_ERROR_INVALID_DATA, responseMsg,
            sizeof(completionCode));
        if (rc)
        {
            error(
                "Failed to encode request firmware date response for endpoint ID '{EID}', response code '{RC}'",
                "EID", eid, "RC", rc);
        }
        return response;
    }

    const auto& componentTuple =
        compImageInfos[applicableComponents[componentIndex]];
    const auto& componentImage = std::get<CompImage>(componentTuple);
    auto compImageSize = componentImage.size();
    debug("Decoded fw request data at offset '{OFFSET}' and length '{LENGTH}' ",
          "OFFSET", offset, "LENGTH", length);
    if (length < PLDM_FWUP_BASELINE_TRANSFER_SIZE || length > maxTransferSize)
    {
        rc = encode_request_firmware_data_resp(
            request->hdr.instance_id, PLDM_FWUP_INVALID_TRANSFER_LENGTH,
            responseMsg, sizeof(completionCode));
        if (rc)
        {
            error(
                "Failed to encode request firmware date response for endpoint ID '{EID}', response code '{RC}'",
                "EID", eid, "RC", rc);
        }
        return response;
    }

    if (offset + length > compImageSize + PLDM_FWUP_BASELINE_TRANSFER_SIZE)
    {
        rc = encode_request_firmware_data_resp(
            request->hdr.instance_id, PLDM_FWUP_DATA_OUT_OF_RANGE, responseMsg,
            sizeof(completionCode));
        if (rc)
        {
            error(
                "Failed to encode request firmware date response for endpoint ID '{EID}', response code '{RC}'",
                "EID", eid, "RC", rc);
        }
        return response;
    }

    size_t padBytes = 0;
    if (offset + length > compImageSize)
    {
        padBytes = offset + length - compImageSize;
    }

    if (componentIndex < progress.size())
    {
        // technically this should never happen, as its an invariant
        // but check to prefer a failed fw update over a pldm crash
        progress[componentIndex].reportFwUpdate(length);
        if (updateManager != nullptr)
        {
            updateManager->updateActivationProgress();
        }
    }

    response.resize(sizeof(pldm_msg_hdr) + sizeof(completionCode) + length);
    responseMsg = new (response.data()) pldm_msg;
    memcpy(response.data() + sizeof(pldm_msg_hdr) + sizeof(completionCode),
           componentImage.data() + offset, length - padBytes);
    rc = encode_request_firmware_data_resp(
        request->hdr.instance_id, completionCode, responseMsg,
        sizeof(completionCode));
    if (rc)
    {
        error(
            "Failed to encode request firmware date response for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
        return response;
    }

    if (!reqFwDataTimer)
    {
        if (offset != 0)
        {
            warning("First data request is not at offset 0");
        }
        createRequestFwDataTimer();
    }

    if (reqFwDataTimer)
    {
        reqFwDataTimer->start(std::chrono::seconds(updateTimeoutSeconds),
                              false);
    }
    else
    {
        error(
            "Failed to start timer for handling request firmware data for endpoint ID {EID}",
            "EID", eid, "RC", rc);
    }

    return response;
}

Response DeviceUpdater::transferComplete(const pldm_msg* request,
                                         size_t payloadLength)
{
    uint8_t completionCode = PLDM_SUCCESS;
    Response response(sizeof(pldm_msg_hdr) + sizeof(completionCode), 0);
    auto responseMsg = new (response.data()) pldm_msg;

    if (reqFwDataTimer)
    {
        reqFwDataTimer->stop();
        reqFwDataTimer.reset();
    }

    uint8_t transferResult = 0;
    auto rc =
        decode_transfer_complete_req(request, payloadLength, &transferResult);
    if (rc)
    {
        error(
            "Failed to decode TransferComplete request for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
        rc = encode_transfer_complete_resp(request->hdr.instance_id,
                                           PLDM_ERROR_INVALID_DATA, responseMsg,
                                           sizeof(completionCode));
        if (rc)
        {
            error(
                "Failed to encode TransferComplete response for endpoint ID '{EID}', response code '{RC}'",
                "EID", eid, "RC", rc);
        }
        return response;
    }

    const auto& componentTuple =
        compImageInfos[applicableComponents[componentIndex]];
    const auto& componentVersion = std::get<CompVersion>(componentTuple);

    if (transferResult == PLDM_FWUP_TRANSFER_SUCCESS)
    {
        info(
            "Component endpoint ID '{EID}' and version '{COMPONENT_VERSION}' transfer complete.",
            "EID", eid, "COMPONENT_VERSION", componentVersion);
    }
    else
    {
        error(
            "Failure in transfer of the component endpoint ID '{EID}' and version '{COMPONENT_VERSION}' with transfer result - {RESULT}",
            "EID", eid, "COMPONENT_VERSION", componentVersion, "RESULT",
            transferResult);
        updateManager->updateDeviceCompletion(eid, false);
        componentUpdateStatus[componentIndex] = false;
        sendCancelUpdateComponentRequest();
    }

    rc = encode_transfer_complete_resp(request->hdr.instance_id, completionCode,
                                       responseMsg, sizeof(completionCode));
    if (rc)
    {
        error(
            "Failed to encode transfer complete response of endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
        return response;
    }

    return response;
}

Response DeviceUpdater::verifyComplete(const pldm_msg* request,
                                       size_t payloadLength)
{
    uint8_t completionCode = PLDM_SUCCESS;
    Response response(sizeof(pldm_msg_hdr) + sizeof(completionCode), 0);
    auto responseMsg = new (response.data()) pldm_msg;

    uint8_t verifyResult = 0;
    auto rc = decode_verify_complete_req(request, payloadLength, &verifyResult);
    if (rc)
    {
        error(
            "Failed to decode verify complete request of endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
        rc = encode_verify_complete_resp(request->hdr.instance_id,
                                         PLDM_ERROR_INVALID_DATA, responseMsg,
                                         sizeof(completionCode));
        if (rc)
        {
            error(
                "Failed to encode verify complete response of endpoint ID '{EID}', response code '{RC}'.",
                "EID", eid, "RC", rc);
        }
        return response;
    }

    const auto& componentTuple =
        compImageInfos[applicableComponents[componentIndex]];
    const auto& componentVersion = std::get<CompVersion>(componentTuple);
    if (componentIndex < progress.size())
    {
        progress[componentIndex].updateState(UpdateProgress::state::Verify);
        if (updateManager != nullptr)
        {
            updateManager->updateActivationProgress();
        }
    }
    if (verifyResult == PLDM_FWUP_VERIFY_SUCCESS)
    {
        info(
            "Component endpoint ID '{EID}' and version '{COMPONENT_VERSION}' verification complete.",
            "EID", eid, "COMPONENT_VERSION", componentVersion);
    }
    else
    {
        error(
            "Failed to verify component endpoint ID '{EID}' and version '{COMPONENT_VERSION}' with transfer result - '{RESULT}'",
            "EID", eid, "COMPONENT_VERSION", componentVersion, "RESULT",
            verifyResult);
        updateManager->updateDeviceCompletion(eid, false);
        componentUpdateStatus[componentIndex] = false;
        sendCancelUpdateComponentRequest();
    }

    rc = encode_verify_complete_resp(request->hdr.instance_id, completionCode,
                                     responseMsg, sizeof(completionCode));
    if (rc)
    {
        error(
            "Failed to encode verify complete response for endpoint ID '{EID}', response code - {RC}",
            "EID", eid, "RC", rc);
        return response;
    }

    return response;
}

Response DeviceUpdater::applyComplete(const pldm_msg* request,
                                      size_t payloadLength)
{
    uint8_t completionCode = PLDM_SUCCESS;
    Response response(sizeof(pldm_msg_hdr) + sizeof(completionCode), 0);
    auto responseMsg = new (response.data()) pldm_msg;

    uint8_t applyResult = 0;
    bitfield16_t compActivationModification{};
    auto rc = decode_apply_complete_req(request, payloadLength, &applyResult,
                                        &compActivationModification);
    if (rc)
    {
        error(
            "Failed to decode apply complete request for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
        rc = encode_apply_complete_resp(request->hdr.instance_id,
                                        PLDM_ERROR_INVALID_DATA, responseMsg,
                                        sizeof(completionCode));
        if (rc)
        {
            error(
                "Failed to encode apply complete response for endpoint ID '{EID}', response code '{RC}'",
                "EID", eid, "RC", rc);
        }
        return response;
    }

    const auto& componentTuple =
        compImageInfos[applicableComponents[componentIndex]];
    const auto& componentVersion = std::get<CompVersion>(componentTuple);

    if (applyResult == PLDM_FWUP_APPLY_SUCCESS ||
        applyResult == PLDM_FWUP_APPLY_SUCCESS_WITH_ACTIVATION_METHOD)
    {
        info(
            "Component endpoint ID '{EID}' with '{COMPONENT_VERSION}' apply complete.",
            "EID", eid, "COMPONENT_VERSION", componentVersion);
        if (componentIndex < progress.size())
        {
            progress[componentIndex].updateState(UpdateProgress::state::Apply);
            if (updateManager != nullptr)
            {
                updateManager->updateActivationProgress();
            }
        }
        if (componentIndex == applicableComponents.size() - 1)
        {
            componentIndex = 0;
            componentUpdateStatus.clear();
            componentUpdateStatus[componentIndex] = true;
            if (updateManager != nullptr)
            {
                pldmRequest = std::make_unique<sdeventplus::source::Defer>(
                    updateManager->event,
                    std::bind(&DeviceUpdater::sendActivateFirmwareRequest,
                              this));
            }
        }
        else
        {
            componentIndex++;
            componentUpdateStatus[componentIndex] = true;
            if (updateManager != nullptr)
            {
                pldmRequest = std::make_unique<sdeventplus::source::Defer>(
                    updateManager->event,
                    std::bind(&DeviceUpdater::sendUpdateComponentRequest, this,
                              componentIndex));
            }
        }
    }
    else
    {
        error(
            "Failed to apply component endpoint ID '{EID}' and version '{COMPONENT_VERSION}', error - {ERROR}",
            "EID", eid, "COMPONENT_VERSION", componentVersion, "ERROR",
            applyResult);
        if (updateManager != nullptr)
        {
            updateManager->updateDeviceCompletion(eid, false);
        }
        componentUpdateStatus[componentIndex] = false;
        sendCancelUpdateComponentRequest();
    }

    rc = encode_apply_complete_resp(request->hdr.instance_id, completionCode,
                                    responseMsg, sizeof(completionCode));
    if (rc)
    {
        error(
            "Failed to encode apply complete response for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
        return response;
    }

    return response;
}

void DeviceUpdater::sendActivateFirmwareRequest()
{
    pldmRequest.reset();
    auto instanceIdResult = updateManager->instanceIdDb.next(eid);
    if (!instanceIdResult)
    {
        throw pldm::InstanceIdError(instanceIdResult.error());
    }
    auto instanceId = instanceIdResult.value();
    Request request(
        sizeof(pldm_msg_hdr) + sizeof(struct pldm_activate_firmware_req));
    auto requestMsg = new (request.data()) pldm_msg;

    auto rc = encode_activate_firmware_req(
        instanceId, PLDM_NOT_ACTIVATE_SELF_CONTAINED_COMPONENTS, requestMsg,
        sizeof(pldm_activate_firmware_req));
    if (rc)
    {
        updateManager->instanceIdDb.free(eid, instanceId);
        error(
            "Failed to encode activate firmware req for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
    }

    rc = updateManager->handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_ACTIVATE_FIRMWARE, std::move(request),
        [this](mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen) {
            this->activateFirmware(eid, response, respMsgLen);
        });
    if (rc)
    {
        error(
            "Failed to send activate firmware request for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
    }
}

void DeviceUpdater::activateFirmware(mctp_eid_t eid, const pldm_msg* response,
                                     size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        // Handle error scenario
        error(
            "No response received for activate firmware for endpoint ID '{EID}'",
            "EID", eid);
        updateManager->updateDeviceCompletion(eid, false);
        return;
    }

    uint8_t completionCode = 0;
    uint16_t estimatedTimeForActivation = 0;

    auto rc = decode_activate_firmware_resp(
        response, respMsgLen, &completionCode, &estimatedTimeForActivation);
    if (rc)
    {
        // Handle error scenario
        error(
            "Failed to decode activate firmware response for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
        updateManager->updateDeviceCompletion(eid, false);
        return;
    }
    if (completionCode)
    {
        // Handle error scenario
        error(
            "Failed to activate firmware response for endpoint ID '{EID}', completion code '{CC}'",
            "EID", eid, "CC", completionCode);
        updateManager->updateDeviceCompletion(eid, false);
        return;
    }

    activationComplete = true;
    if (updateManager == nullptr)
    {
        return;
    }

    updateManager->updateActivationProgress();
    updateManager->updateDeviceCompletion(eid, true);
}

void DeviceUpdater::sendCancelUpdateComponentRequest()
{
    pldmRequest.reset();
    auto instanceIdResult = updateManager->instanceIdDb.next(eid);
    if (!instanceIdResult)
    {
        throw pldm::InstanceIdError(instanceIdResult.error());
    }
    auto instanceId = instanceIdResult.value();
    Request request(sizeof(pldm_msg_hdr));
    auto requestMsg = new (request.data()) pldm_msg;

    auto rc = encode_cancel_update_component_req(
        instanceId, requestMsg, PLDM_CANCEL_UPDATE_COMPONENT_REQ_BYTES);
    if (rc)
    {
        updateManager->instanceIdDb.free(eid, instanceId);
        error(
            "Failed to encode cancel update component request for endpoint ID '{EID}', component index '{COMPONENT_INDEX}', response code '{RC}'",
            "EID", eid, "COMPONENT_INDEX", componentIndex, "RC", rc);
        return;
    }

    rc = updateManager->handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_CANCEL_UPDATE_COMPONENT,
        std::move(request),
        [this](mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen) {
            this->cancelUpdateComponent(eid, response, respMsgLen);
        });
    if (rc)
    {
        error(
            "Failed to send cancel update component request for endpoint ID '{EID}', component index '{COMPONENT_INDEX}', response code '{RC}'",
            "EID", eid, "COMPONENT_INDEX", componentIndex, "RC", rc);
    }
}

void DeviceUpdater::cancelUpdateComponent(
    mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen)
{
    // Check if response is valid
    if (response == nullptr || !respMsgLen)
    {
        error(
            "No response received for cancel update component for endpoint ID '{EID}'",
            "EID", eid);
        return;
    }

    uint8_t completionCode = 0;
    auto rc = decode_cancel_update_component_resp(response, respMsgLen,
                                                  &completionCode);
    if (rc)
    {
        error(
            "Failed to decode cancel update component response for endpoint ID '{EID}', component index '{COMPONENT_INDEX}', completion code '{CC}'",
            "EID", eid, "COMPONENT_INDEX", componentIndex, "CC",
            completionCode);
        return;
    }
    if (completionCode)
    {
        error(
            "Failed to cancel update component for endpoint ID '{EID}', component index '{COMPONENT_INDEX}', completion code '{CC}'",
            "EID", eid, "COMPONENT_INDEX", componentIndex, "CC",
            completionCode);
        return;
    }

    // Check if this is the last component being cancelled
    if (componentIndex == applicableComponents.size() - 1)
    {
        for (auto& compStatus : componentUpdateStatus)
        {
            if (compStatus.second)
            {
                // If at least one component update succeeded, proceed with
                // activation
                componentIndex = 0;
                componentUpdateStatus.clear();
                pldmRequest = std::make_unique<sdeventplus::source::Defer>(
                    updateManager->event,
                    std::bind(&DeviceUpdater::sendActivateFirmwareRequest,
                              this));
                return;
            }
        }
        updateManager->updateDeviceCompletion(eid, false);
    }
    else
    {
        // Move to next component and update its status
        componentIndex++;
        componentUpdateStatus[componentIndex] = true;
        pldmRequest = std::make_unique<sdeventplus::source::Defer>(
            updateManager->event,
            std::bind(&DeviceUpdater::sendUpdateComponentRequest, this,
                      componentIndex));
    }
    return;
}

uint8_t DeviceUpdater::getProgress() const
{
    if (progress.empty())
    {
        return 0;
    }

    if (activationComplete)
    {
        return firmwareActivationProgressPercent;
    }

    uint64_t totalSize = 0;
    uint64_t weightedProgress = 0;
    for (const auto& el : progress)
    {
        totalSize += el.getTotalSize();
        weightedProgress +=
            el.getTotalSize() * static_cast<uint64_t>(el.getProgress());
    }

    if (totalSize == 0)
    {
        error("total component size is 0 on eid {EID}", "EID", eid);
        return 0;
    }

    const double percentage =
        static_cast<double>(weightedProgress) / static_cast<double>(totalSize);

    return static_cast<uint8_t>(std::floor(percentage));
}

} // namespace fw_update

} // namespace pldm
