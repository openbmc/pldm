#include "device_updater.hpp"

#include "libpldm/firmware_update.h"

#include "activation.hpp"
#include "update_manager.hpp"

#include <functional>

namespace pldm
{

namespace fw_update
{

void DeviceUpdater::startFwUpdateFlow()
{
    auto instanceId = updateManager->requester.getInstanceId(eid);
    // NumberOfComponents
    const auto& applicableComponents =
        std::get<ApplicableComponents>(fwDeviceIDRecord);
    // PackageDataLength
    const auto& fwDevicePkgData =
        std::get<FirmwareDevicePackageData>(fwDeviceIDRecord);
    // ComponentImageSetVersionString
    const auto& compImageSetVersion =
        std::get<ComponentImageSetVersion>(fwDeviceIDRecord);
    variable_field compImgSetVerStrInfo{};
    compImgSetVerStrInfo.ptr =
        reinterpret_cast<const uint8_t*>(compImageSetVersion.data());
    compImgSetVerStrInfo.length =
        static_cast<uint8_t>(compImageSetVersion.size());

    Request request(sizeof(pldm_msg_hdr) +
                    sizeof(struct pldm_request_update_req) +
                    compImgSetVerStrInfo.length);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());

    auto rc = encode_request_update_req(
        instanceId, maxTransferSize, applicableComponents.size(),
        PLDM_FWUP_MIN_OUTSTANDING_REQ, fwDevicePkgData.size(),
        PLDM_STR_TYPE_ASCII, compImgSetVerStrInfo.length, &compImgSetVerStrInfo,
        requestMsg,
        sizeof(struct pldm_request_update_req) + compImgSetVerStrInfo.length);
    if (rc)
    {
        updateManager->requester.markFree(eid, instanceId);
        std::cerr << "encode_request_update_req failed, EID=" << unsigned(eid)
                  << ", RC=" << rc << "\n";
        // Handle error scenario
    }

    rc = updateManager->handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_REQUEST_UPDATE, std::move(request),
        std::move(std::bind_front(&DeviceUpdater::requestUpdate, this)));
    if (rc)
    {
        std::cerr << "Failed to send RequestUpdate request, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n ";
        // Handle error scenario
    }
}

void DeviceUpdater::requestUpdate(mctp_eid_t eid, const pldm_msg* response,
                                  size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        // Handle error scenario
        const auto& applicableComponents =
            std::get<ApplicableComponents>(fwDeviceIDRecord);
        for (size_t comp_index = 0; comp_index < applicableComponents.size();
             ++comp_index)
        {
            auto messageID = updateManager->TransferFailed;
            updateManager->CreateMessageRegistery(eid, fwDeviceIDRecord,
                                                  comp_index, messageID);
        }
        updateManager->updateDeviceCompletion(eid, false);

        std::cerr << "No response received for RequestUpdate, EID="
                  << unsigned(eid) << "\n";
        return;
    }

    uint8_t completionCode = 0;
    uint16_t fdMetaDataLen = 0;
    uint8_t fdWillSendPkgData = 0;

    auto rc = decode_request_update_resp(response, respMsgLen, &completionCode,
                                         &fdMetaDataLen, &fdWillSendPkgData);
    if (rc)
    {
        std::cerr << "Decoding RequestUpdate response failed, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
        return;
    }
    if (completionCode)
    {
        // Transfer Failed - request update ALREADY_IN_UPDATE_MODE,
        // UNABLE_TO_INITIATE_UPDATE Report Message Registeries for Each
        // component targeted for FD/EID
        const auto& applicableComponents =
            std::get<ApplicableComponents>(fwDeviceIDRecord);
        for (size_t comp_index = 0; comp_index < applicableComponents.size();
             ++comp_index)
        {
            auto messageID = updateManager->TransferFailed;
            updateManager->CreateMessageRegistery(eid, fwDeviceIDRecord,
                                                  comp_index, messageID);
        }
        std::cerr << "RequestUpdate response failed with error "
                     "completion code, EID="
                  << unsigned(eid) << ", CC=" << unsigned(completionCode)
                  << "\n";

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

    auto instanceId = updateManager->requester.getInstanceId(eid);
    // TransferFlag
    const auto& applicableComponents =
        std::get<ApplicableComponents>(fwDeviceIDRecord);
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
    const auto& comp = compImageInfos[applicableComponents[offset]];
    // ComponentClassification
    CompClassification compClassification = std::get<static_cast<size_t>(
        ComponentImageInfoPos::CompClassificationPos)>(comp);
    // ComponentIdentifier
    CompIdentifier compIdentifier =
        std::get<static_cast<size_t>(ComponentImageInfoPos::CompIdentifierPos)>(
            comp);
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
    }
    // ComponentComparisonStamp
    CompComparisonStamp compComparisonStamp = std::get<static_cast<size_t>(
        ComponentImageInfoPos::CompComparisonStampPos)>(comp);
    // ComponentVersionString
    const auto& compVersion =
        std::get<static_cast<size_t>(ComponentImageInfoPos::CompVersionPos)>(
            comp);
    variable_field compVerStrInfo{};
    compVerStrInfo.ptr = reinterpret_cast<const uint8_t*>(compVersion.data());
    compVerStrInfo.length = static_cast<uint8_t>(compVersion.size());

    Request request(sizeof(pldm_msg_hdr) +
                    sizeof(struct pldm_pass_component_table_req) +
                    compVerStrInfo.length);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
    auto rc = encode_pass_component_table_req(
        instanceId, transferFlag, compClassification, compIdentifier,
        compClassificationIndex, compComparisonStamp, PLDM_STR_TYPE_ASCII,
        compVerStrInfo.length, &compVerStrInfo, requestMsg,
        sizeof(pldm_pass_component_table_req) + compVerStrInfo.length);
    if (rc)
    {
        updateManager->requester.markFree(eid, instanceId);
        std::cerr << "encode_pass_component_table_req failed, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
        // Handle error scenario
    }

    rc = updateManager->handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_PASS_COMPONENT_TABLE,
        std::move(request),
        std::move(std::bind_front(&DeviceUpdater::passCompTable, this)));
    if (rc)
    {
        std::cerr << "Failed to send PassComponentTable request, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n ";
        // Handle error scenario
    }
}

void DeviceUpdater::passCompTable(mctp_eid_t eid, const pldm_msg* response,
                                  size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        // Handle error scenario
        auto messageID = updateManager->TransferFailed;
        updateManager->CreateMessageRegistery(eid, fwDeviceIDRecord,
                                              componentIndex, messageID);

        updateManager->updateDeviceCompletion(eid, false);

        std::cerr << "No response received for PassComponentTable, EID="
                  << unsigned(eid) << "\n";
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
        std::cerr << "Decoding PassComponentTable response failed, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
        return;
    }
    if (completionCode)
    {
        // Handle error scenario
        std::cerr << "PassComponentTable response failed with error "
                     "completion code, EID="
                  << unsigned(eid) << ", CC=" << unsigned(completionCode)
                  << "\n";
        return;
    }
    // Handle ComponentResponseCode
    if (compResponse)
    {
        // Component Comparision Stamp Equal or Lower version Reported
        // PLDM_CRC_COMP_COMPARISON_STAMP_IDENTICAL,PLDM_CRC_COMP_COMPARISON_STAMP_LOWER
        // PLDM_CRC_COMP_SECURITY_RESTRICTIONS
        auto messageID = updateManager->TransferFailed;
        updateManager->CreateMessageRegistery(eid, fwDeviceIDRecord,
                                              componentIndex, messageID);

        // logging CompResponseCode
        std::cerr << "PassComponentTable response received "
                     "compResponseCode code, EID="
                  << unsigned(eid)
                  << ", CompResponseCode=" << unsigned(compResponseCode)
                  << "\n";

        updateManager->updateDeviceCompletion(eid, false);
        return;
    }

    const auto& applicableComponents =
        std::get<ApplicableComponents>(fwDeviceIDRecord);
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

    auto instanceId = updateManager->requester.getInstanceId(eid);
    const auto& applicableComponents =
        std::get<ApplicableComponents>(fwDeviceIDRecord);
    const auto& comp = compImageInfos[applicableComponents[offset]];
    // ComponentClassification
    CompClassification compClassification = std::get<static_cast<size_t>(
        ComponentImageInfoPos::CompClassificationPos)>(comp);
    // ComponentIdentifier
    CompIdentifier compIdentifier =
        std::get<static_cast<size_t>(ComponentImageInfoPos::CompIdentifierPos)>(
            comp);
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
    }

    // UpdateOptionFlags
    bitfield32_t updateOptionFlags;
    updateOptionFlags.bits.bit0 = std::get<3>(comp)[0];
    // ComponentVersion
    const auto& compVersion = std::get<7>(comp);
    variable_field compVerStrInfo{};
    compVerStrInfo.ptr = reinterpret_cast<const uint8_t*>(compVersion.data());
    compVerStrInfo.length = static_cast<uint8_t>(compVersion.size());

    Request request(sizeof(pldm_msg_hdr) +
                    sizeof(struct pldm_update_component_req) +
                    compVerStrInfo.length);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());

    auto rc = encode_update_component_req(
        instanceId, compClassification, compIdentifier, compClassificationIndex,
        std::get<static_cast<size_t>(
            ComponentImageInfoPos::CompComparisonStampPos)>(comp),
        std::get<static_cast<size_t>(ComponentImageInfoPos::CompSizePos)>(comp),
        updateOptionFlags, PLDM_STR_TYPE_ASCII, compVerStrInfo.length,
        &compVerStrInfo, requestMsg,
        sizeof(pldm_update_component_req) + compVerStrInfo.length);
    if (rc)
    {
        updateManager->requester.markFree(eid, instanceId);
        std::cerr << "encode_update_component_req failed, EID=" << unsigned(eid)
                  << ", RC=" << rc << "\n";
        // Handle error scenario
    }

    rc = updateManager->handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_UPDATE_COMPONENT, std::move(request),
        std::move(std::bind_front(&DeviceUpdater::updateComponent, this)));
    if (rc)
    {
        std::cerr << "Failed to send UpdateComponent request, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n ";
        // Handle error scenario
    }
}

void DeviceUpdater::updateComponent(mctp_eid_t eid, const pldm_msg* response,
                                    size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        // Handle error scenario
        auto messageID = updateManager->TransferFailed;
        updateManager->CreateMessageRegistery(eid, fwDeviceIDRecord,
                                              componentIndex, messageID);

        updateManager->updateDeviceCompletion(eid, false);

        std::cerr << "No response received for updateComponent, EID="
                  << unsigned(eid) << "\n";
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
        std::cerr << "Decoding UpdateComponent response failed, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
        return;
    }
    if (completionCode)
    {
        std::cerr << "UpdateComponent response failed with error "
                     "completion code, EID="
                  << unsigned(eid) << ", CC=" << unsigned(completionCode)
                  << "\n";
        return;
    }
    if (compCompatibilityResp)
    {
        // Component compatibility response code
        // PLDM_CRC_COMP_SECURITY_RESTRICTIONS
        auto messageID = updateManager->TransferFailed;
        updateManager->CreateMessageRegistery(eid, fwDeviceIDRecord,
                                              componentIndex, messageID);

        // logging CompResponseCode
        std::cerr << "Update Component response received "
                     "compCompatibilityRespCode code, EID="
                  << unsigned(eid) << ", compCompatibilityRespCode="
                  << unsigned(compCompatibilityRespCode) << "\n";

        updateManager->updateDeviceCompletion(eid, false);
        return;
    }

    // Reporting Message Registeries for Transferring to Component
    auto messageID = updateManager->TransferringToComponent;
    updateManager->CreateMessageRegistery(eid, fwDeviceIDRecord, componentIndex,
                                          messageID);
}

Response DeviceUpdater::requestFwData(const pldm_msg* request,
                                      size_t payloadLength)
{
    uint8_t completionCode = PLDM_SUCCESS;
    uint32_t offset = 0;
    uint32_t length = 0;
    Response response(sizeof(pldm_msg_hdr) + sizeof(completionCode), 0);
    auto responseMsg = reinterpret_cast<pldm_msg*>(response.data());
    auto rc = decode_request_firmware_data_req(request, payloadLength, &offset,
                                               &length);
    if (rc)
    {
        std::cerr << "Decoding RequestFirmwareData request failed, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";

        // Request FirmwareData Failed
        auto messageID = updateManager->TransferFailed;
        updateManager->CreateMessageRegistery(eid, fwDeviceIDRecord,
                                              componentIndex, messageID);

        rc = encode_request_firmware_data_resp(
            request->hdr.instance_id, PLDM_ERROR_INVALID_DATA, responseMsg,
            sizeof(completionCode));
        if (rc)
        {
            std::cerr << "Encoding RequestFirmwareData response failed, EID="
                      << unsigned(eid) << ", RC=" << rc << "\n";
        }

        updateManager->updateDeviceCompletion(eid, false);
        return response;
    }

    const auto& applicableComponents =
        std::get<ApplicableComponents>(fwDeviceIDRecord);
    const auto& comp = compImageInfos[applicableComponents[componentIndex]];
    auto compOffset = std::get<5>(comp);
    auto compSize = std::get<6>(comp);
    std::cerr << "offset = " << unsigned(offset)
              << ", length = " << unsigned(length) << "\n";

    if (length < PLDM_FWUP_BASELINE_TRANSFER_SIZE || length > maxTransferSize)
    {
        std::cerr << "RequestFirmwareData reporeted \
						PLDM_FWUP_INVALID_TRANSFER_LENGTH, EID="
                  << unsigned(eid) << ", length=" << length << "\n";

        rc = encode_request_firmware_data_resp(
            request->hdr.instance_id, PLDM_FWUP_INVALID_TRANSFER_LENGTH,
            responseMsg, sizeof(completionCode));
        if (rc)
        {
            std::cerr << "Encoding RequestFirmwareData response failed, EID="
                      << unsigned(eid) << ", RC=" << rc << "\n";
        }
        return response;
    }

    if (offset + length > compSize + PLDM_FWUP_BASELINE_TRANSFER_SIZE)
    {
        std::cerr << "RequestFirmwareData reporeted \
						PLDM_FWUP_DATA_OUT_OF_RANGE, EID="
                  << unsigned(eid) << ", offset=" << offset << ", length"
                  << length << "\n";

        rc = encode_request_firmware_data_resp(
            request->hdr.instance_id, PLDM_FWUP_DATA_OUT_OF_RANGE, responseMsg,
            sizeof(completionCode));
        if (rc)
        {
            std::cerr << "Encoding RequestFirmwareData response failed, EID="
                      << unsigned(eid) << ", RC=" << rc << "\n";
        }
        return response;
    }

    size_t padBytes = 0;
    if (offset + length > compSize)
    {
        padBytes = offset + length - compSize;
    }

    response.resize(sizeof(pldm_msg_hdr) + sizeof(completionCode) + length);
    responseMsg = reinterpret_cast<pldm_msg*>(response.data());
    package.seekg(compOffset + offset);
    package.read(reinterpret_cast<char*>(response.data() +
                                         sizeof(pldm_msg_hdr) +
                                         sizeof(completionCode)),
                 length - padBytes);
    rc = encode_request_firmware_data_resp(request->hdr.instance_id,
                                           completionCode, responseMsg,
                                           sizeof(completionCode));
    if (rc)
    {
        std::cerr << "Encoding RequestFirmwareData response failed, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
        return response;
    }

    return response;
}

Response DeviceUpdater::transferComplete(const pldm_msg* request,
                                         size_t payloadLength)
{
    uint8_t completionCode = PLDM_SUCCESS;
    Response response(sizeof(pldm_msg_hdr) + sizeof(completionCode), 0);
    auto responseMsg = reinterpret_cast<pldm_msg*>(response.data());

    uint8_t transferResult = 0;
    auto rc =
        decode_transfer_complete_req(request, payloadLength, &transferResult);
    if (rc)
    {
        std::cerr << "Decoding TransferComplete request failed, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
        rc = encode_transfer_complete_resp(request->hdr.instance_id,
                                           PLDM_ERROR_INVALID_DATA, responseMsg,
                                           sizeof(completionCode));
        if (rc)
        {
            std::cerr << "Encoding TransferComplete response failed, EID="
                      << unsigned(eid) << ", RC=" << rc << "\n";
        }
        return response;
    }

    const auto& applicableComponents =
        std::get<ApplicableComponents>(fwDeviceIDRecord);
    const auto& comp = compImageInfos[applicableComponents[componentIndex]];
    const auto& compVersion =
        std::get<static_cast<size_t>(ComponentImageInfoPos::CompVersionPos)>(
            comp);

    if (transferResult == PLDM_FWUP_TRANSFER_SUCCESS)
    {
        std::cout << "Component Transfer complete, EID=" << unsigned(eid)
                  << ", COMPONENT_VERSION=" << compVersion << "\n";
    }
    else
    {
        // Transfer Complete Failed
        auto messageID = updateManager->TransferFailed;
        updateManager->CreateMessageRegistery(eid, fwDeviceIDRecord,
                                              componentIndex, messageID);
        std::cerr << "Transfer of the component failed, EID=" << unsigned(eid)
                  << ", COMPONENT_VERSION=" << compVersion
                  << ", TRANSFER_RESULT=" << unsigned(transferResult) << "\n";

        updateManager->updateDeviceCompletion(eid, false);
        return response;
    }

    rc = encode_transfer_complete_resp(request->hdr.instance_id, completionCode,
                                       responseMsg, sizeof(completionCode));
    if (rc)
    {
        std::cerr << "Encoding TransferComplete response failed, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
        return response;
    }

    return response;
}

Response DeviceUpdater::verifyComplete(const pldm_msg* request,
                                       size_t payloadLength)
{
    uint8_t completionCode = PLDM_SUCCESS;
    Response response(sizeof(pldm_msg_hdr) + sizeof(completionCode), 0);
    auto responseMsg = reinterpret_cast<pldm_msg*>(response.data());

    uint8_t verifyResult = 0;
    auto rc = decode_verify_complete_req(request, payloadLength, &verifyResult);
    if (rc)
    {
        std::cerr << "Decoding VerifyComplete request failed, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
        rc = encode_verify_complete_resp(request->hdr.instance_id,
                                         PLDM_ERROR_INVALID_DATA, responseMsg,
                                         sizeof(completionCode));
        if (rc)
        {
            std::cerr << "Encoding VerifyComplete response failed, EID="
                      << unsigned(eid) << ", RC=" << rc << "\n";
        }
        return response;
    }

    const auto& applicableComponents =
        std::get<ApplicableComponents>(fwDeviceIDRecord);
    const auto& comp = compImageInfos[applicableComponents[componentIndex]];
    const auto& compVersion =
        std::get<static_cast<size_t>(ComponentImageInfoPos::CompVersionPos)>(
            comp);

    if (verifyResult == PLDM_FWUP_VERIFY_SUCCESS)
    {
        std::cout << "Component verification complete, EID=" << unsigned(eid)
                  << ", COMPONENT_VERSION=" << compVersion << "\n";
    }
    else
    {
        // Verify complete Failed
        auto messageID = updateManager->VerificationFailed;
        updateManager->CreateMessageRegistery(eid, fwDeviceIDRecord,
                                              componentIndex, messageID);
        std::cerr << "Component verification failed, EID=" << unsigned(eid)
                  << ", COMPONENT_VERSION=" << compVersion
                  << ", VERIFY_RESULT=" << unsigned(verifyResult) << "\n";

        updateManager->updateDeviceCompletion(eid, false);
        return response;
    }

    rc = encode_verify_complete_resp(request->hdr.instance_id, completionCode,
                                     responseMsg, sizeof(completionCode));
    if (rc)
    {
        std::cerr << "Encoding VerifyComplete response failed, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
        return response;
    }

    return response;
}

Response DeviceUpdater::applyComplete(const pldm_msg* request,
                                      size_t payloadLength)
{
    uint8_t completionCode = PLDM_SUCCESS;
    Response response(sizeof(pldm_msg_hdr) + sizeof(completionCode), 0);
    auto responseMsg = reinterpret_cast<pldm_msg*>(response.data());

    uint8_t applyResult = 0;
    bitfield16_t compActivationModification{};

    auto rc = decode_apply_complete_req(request, payloadLength, &applyResult,
                                        &compActivationModification);
    if (rc)
    {
        std::cerr << "Decoding ApplyComplete request failed, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
        rc = encode_apply_complete_resp(request->hdr.instance_id,
                                        PLDM_ERROR_INVALID_DATA, responseMsg,
                                        sizeof(completionCode));
        if (rc)
        {
            std::cerr << "Encoding ApplyComplete response failed, EID="
                      << unsigned(eid) << ", RC=" << rc << "\n";
        }
        return response;
    }

    const auto& applicableComponents =
        std::get<ApplicableComponents>(fwDeviceIDRecord);
    const auto& comp = compImageInfos[applicableComponents[componentIndex]];
    const auto& compVersion =
        std::get<static_cast<size_t>(ComponentImageInfoPos::CompVersionPos)>(
            comp);

    if (applyResult == PLDM_FWUP_APPLY_SUCCESS ||
        applyResult == PLDM_FWUP_APPLY_SUCCESS_WITH_ACTIVATION_METHOD)
    {
        // Reporting update Successful
        auto messageID = updateManager->UpdateSuccessful;
        updateManager->CreateMessageRegistery(eid, fwDeviceIDRecord,
                                              componentIndex, messageID);
        std::cout << "Component apply complete, EID=" << unsigned(eid)
                  << ", COMPONENT_VERSION=" << compVersion << "\n";
        updateManager->updateActivationProgress();

        // AwaitToActivate
        messageID = updateManager->AwaitToActivate;
        static constexpr auto resolution = "Power reset is needed!";
        updateManager->CreateMessageRegistery(
            eid, fwDeviceIDRecord, componentIndex, messageID, resolution);
    }
    else
    {
        // Apply Complete Failed
        auto messageID = updateManager->ApplyFailed;
        updateManager->CreateMessageRegistery(eid, fwDeviceIDRecord,
                                              componentIndex, messageID);
        std::cerr << "Component apply failed, EID=" << unsigned(eid)
                  << ", COMPONENT_VERSION=" << compVersion
                  << ", APPLY_RESULT=" << unsigned(applyResult) << "\n";

        updateManager->updateDeviceCompletion(eid, false);
        return response;
    }

    rc = encode_apply_complete_resp(request->hdr.instance_id, completionCode,
                                    responseMsg, sizeof(completionCode));
    if (rc)
    {
        std::cerr << "Encoding ApplyComplete response failed, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
        return response;
    }

    if (componentIndex == applicableComponents.size() - 1)
    {
        componentIndex = 0;
        pldmRequest = std::make_unique<sdeventplus::source::Defer>(
            updateManager->event,
            std::bind(&DeviceUpdater::sendActivateFirmwareRequest, this));
    }
    else
    {
        componentIndex++;
        pldmRequest = std::make_unique<sdeventplus::source::Defer>(
            updateManager->event,
            std::bind(&DeviceUpdater::sendUpdateComponentRequest, this,
                      componentIndex));
    }

    return response;
}

void DeviceUpdater::sendActivateFirmwareRequest()
{
    pldmRequest.reset();
    auto instanceId = updateManager->requester.getInstanceId(eid);
    Request request(sizeof(pldm_msg_hdr) +
                    sizeof(struct pldm_activate_firmware_req));
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());

    auto rc = encode_activate_firmware_req(
        instanceId, PLDM_NOT_ACTIVATE_SELF_CONTAINED_COMPONENTS, requestMsg,
        sizeof(pldm_activate_firmware_req));
    if (rc)
    {
        updateManager->requester.markFree(eid, instanceId);
        std::cerr << "encode_activate_firmware_req failed, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
    }

    rc = updateManager->handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_ACTIVATE_FIRMWARE, std::move(request),
        std::move(std::bind_front(&DeviceUpdater::activateFirmware, this)));
    if (rc)
    {
        std::cerr << "Failed to send ActivateFirmware request, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n ";
    }
}

void DeviceUpdater::activateFirmware(mctp_eid_t eid, const pldm_msg* response,
                                     size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        // Handle error scenario
        std::cerr << "No response received for ActivateFirmware, EID="
                  << unsigned(eid) << "\n";
        return;
    }

    uint8_t completionCode = 0;
    uint16_t estimatedTimeForActivation = 0;

    auto rc = decode_activate_firmware_resp(
        response, respMsgLen, &completionCode, &estimatedTimeForActivation);
    if (rc)
    {
        // Handle error scenario
        std::cerr << "Decoding ActivateFirmware response failed, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
        return;
    }

    if (completionCode)
    {
        // Handle error scenario
        // Activation Failed
        auto messageID = updateManager->ActivateFailed;
        updateManager->CreateMessageRegistery(eid, fwDeviceIDRecord,
                                              componentIndex, messageID);

        std::cerr << "ActivateFirmware response failed with error "
                     "completion code, EID="
                  << unsigned(eid) << ", CC=" << unsigned(completionCode)
                  << "\n";
        return;
    }

    // AwaitToActivate
    // static constexpr auto messageID = "Update.1.0.AwaitToActivate";
    // updateManager->CreateMessageRegistery(eid, fwDeviceIDRecord,
    //                                        componentIndex, messageID);

    updateManager->updateDeviceCompletion(eid, true);
}

} // namespace fw_update

} // namespace pldm
