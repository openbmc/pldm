#include "fw_device_updater.hpp"

#include "libpldm/firmware_update.h"

#include <functional>

namespace pldm
{

namespace fw_update
{

void DeviceUpdater::startFwUpdateFlow()
{
    auto instanceId = requester.getInstanceId(eid);
    const auto& compImageSetVersion =
        std::get<ComponentImageSetVersion>(fwDeviceIDRecord);
    variable_field compImgSetVerStrInfo{};
    compImgSetVerStrInfo.ptr =
        reinterpret_cast<const uint8_t*>(compImageSetVersion.data());
    compImgSetVerStrInfo.length =
        static_cast<uint8_t>(compImageSetVersion.size());
    const auto& fwDevicePkgData =
        std::get<FirmwareDevicePackageData>(fwDeviceIDRecord);

    Request request(sizeof(pldm_msg_hdr) +
                    sizeof(struct pldm_request_update_req) +
                    compImgSetVerStrInfo.length);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());

    auto rc = encode_request_update_req(
        instanceId, maxTransferSize, applicableCompOffsets.size(),
        PLDM_FWUP_MIN_OUTSTANDING_REQ, fwDevicePkgData.size(),
        PLDM_STR_TYPE_ASCII, compImgSetVerStrInfo.length, &compImgSetVerStrInfo,
        requestMsg,
        sizeof(struct pldm_request_update_req) + compImgSetVerStrInfo.length);
    if (rc)
    {
        requester.markFree(eid, instanceId);
        std::cerr << "encode_request_update_req failed, EID=" << unsigned(eid)
                  << ", RC=" << rc << "\n";
        // Stop any timers, log
    }

    rc = handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_REQUEST_UPDATE, std::move(request),
        std::move(std::bind_front(&DeviceUpdater::requestUpdate, this)));
    if (rc)
    {
        std::cerr << "Failed to send RequestUpdate request, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n ";
    }
}

void DeviceUpdater::requestUpdate(mctp_eid_t eid, const pldm_msg* response,
                                  size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        // Stop any timers, log and update UpdateManager with failure
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
        std::cerr << "RequestUpdate response failed with error "
                     "completion code, EID="
                  << unsigned(eid) << ", CC=" << unsigned(completionCode)
                  << "\n";
        return;
    }
    // Optional metadata and GetPackageData not handled
    pldmRequest = std::make_unique<sdeventplus::source::Defer>(
        event, std::bind(&DeviceUpdater::sendPassCompTableRequest, this,
                         componentIndex));
}

void DeviceUpdater::sendPassCompTableRequest(size_t offset)
{
    pldmRequest.reset();
    auto instanceId = requester.getInstanceId(eid);
    const auto& comp = compImageInfos[applicableCompOffsets[offset]];
    CompClassification compClassification = std::get<0>(comp);
    CompIdentifier compIdentifier = std::get<1>(comp);
    CompClassificationIndex compClassificationIndex{};
    auto compKey = std::make_pair(compClassification, compIdentifier);
    if (compInfo.contains(compKey))
    {
        auto search = compInfo.find(compKey);
        compClassificationIndex = search->second;
    }
    const auto& compVersion = std::get<7>(comp);
    variable_field compVerStrInfo{};
    compVerStrInfo.ptr = reinterpret_cast<const uint8_t*>(compVersion.data());
    compVerStrInfo.length = static_cast<uint8_t>(compVersion.size());

    Request request(sizeof(pldm_msg_hdr) +
                    sizeof(struct pldm_pass_component_table_req) +
                    compVerStrInfo.length);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());

    uint8_t transferFlag = 0;
    if (applicableCompOffsets.size() == 1)
    {
        transferFlag = PLDM_START_AND_END;
    }
    else if (offset == 0)
    {
        transferFlag = PLDM_START;
    }
    else if (offset == applicableCompOffsets.size() - 1)
    {
        transferFlag = PLDM_END;
    }
    else
    {
        transferFlag = PLDM_MIDDLE;
    }

    auto rc = encode_pass_component_table_req(
        instanceId, transferFlag, compClassification, compIdentifier,
        compClassificationIndex, std::get<2>(comp), PLDM_STR_TYPE_ASCII,
        compVerStrInfo.length, &compVerStrInfo, requestMsg,
        sizeof(pldm_pass_component_table_req) + compVerStrInfo.length);
    if (rc)
    {
        requester.markFree(eid, instanceId);
        std::cerr << "encode_pass_component_table_req failed, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
        // Stop any timers, log and update UpdateManager with failure
    }

    rc = handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_PASS_COMPONENT_TABLE,
        std::move(request),
        std::move(std::bind_front(&DeviceUpdater::passCompTable, this)));
    if (rc)
    {
        std::cerr << "Failed to send PassComponentTable request, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n ";
    }
}

void DeviceUpdater::passCompTable(mctp_eid_t eid, const pldm_msg* response,
                                  size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        // Stop any timers, log and update UpdateManager with failure
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
        std::cerr << "Decoding PassComponentTable response failed, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
        return;
    }
    if (completionCode)
    {
        std::cerr << "PassComponentTable response failed with error "
                     "completion code, EID="
                  << unsigned(eid) << ", CC=" << unsigned(completionCode)
                  << "\n";
        return;
    }

    // Optional metadata and GetPackageData not handled
    if (componentIndex == applicableCompOffsets.size())
    {
        componentIndex = 0;
        pldmRequest = std::make_unique<sdeventplus::source::Defer>(
            event, std::bind(&DeviceUpdater::sendUpdateComponentRequest, this,
                             componentIndex));
        sendUpdateComponentRequest(componentIndex);
    }
    else
    {
        componentIndex++;
        pldmRequest = std::make_unique<sdeventplus::source::Defer>(
            event, std::bind(&DeviceUpdater::sendPassCompTableRequest, this,
                             componentIndex));
    }
}

void DeviceUpdater::sendUpdateComponentRequest(size_t offset)
{
    pldmRequest.reset();
    auto instanceId = requester.getInstanceId(eid);
    const auto& comp = compImageInfos[applicableCompOffsets[offset]];
    CompClassification compClassification = std::get<0>(comp);
    CompIdentifier compIdentifier = std::get<1>(comp);
    CompClassificationIndex compClassificationIndex{};
    auto compKey = std::make_pair(compClassification, compIdentifier);
    if (compInfo.contains(compKey))
    {
        auto search = compInfo.find(compKey);
        compClassificationIndex = search->second;
    }
    const auto& compVersion = std::get<7>(comp);
    variable_field compVerStrInfo{};
    compVerStrInfo.ptr = reinterpret_cast<const uint8_t*>(compVersion.data());
    compVerStrInfo.length = static_cast<uint8_t>(compVersion.size());
    bitfield32_t updateOptionFlags;
    updateOptionFlags.bits.bit0 = std::get<3>(comp)[0];

    Request request(sizeof(pldm_msg_hdr) +
                    sizeof(struct pldm_update_component_req) +
                    compVerStrInfo.length);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());

    auto rc = encode_update_component_req(
        instanceId, compClassification, compIdentifier, compClassificationIndex,
        std::get<2>(comp), std::get<2>(comp), updateOptionFlags,
        PLDM_STR_TYPE_ASCII, compVerStrInfo.length, &compVerStrInfo, requestMsg,
        sizeof(pldm_update_component_req) + compVerStrInfo.length);
    if (rc)
    {
        requester.markFree(eid, instanceId);
        std::cerr << "encode_update_component_req failed, EID=" << unsigned(eid)
                  << ", RC=" << rc << "\n";
        // Stop any timers, log and update UpdateManager with failure
    }

    rc = handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_UPDATE_COMPONENT, std::move(request),
        std::move(std::bind_front(&DeviceUpdater::updateComponent, this)));
    if (rc)
    {
        std::cerr << "Failed to send UpdateComponent request, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n ";
    }
}

void DeviceUpdater::updateComponent(mctp_eid_t eid, const pldm_msg* response,
                                    size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        // Stop any timers, log and update UpdateManager with failure
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
        rc = encode_request_firmware_data_resp(
            request->hdr.instance_id, PLDM_ERROR_INVALID_DATA, responseMsg,
            sizeof(completionCode));
        if (rc)
        {
            std::cerr << "Encoding RequestFirmwareData response failed, EID="
                      << unsigned(eid) << ", RC=" << rc << "\n";
        }
        return response;
    }

    const auto& comp = compImageInfos[applicableCompOffsets[componentIndex]];
    auto compOffset = std::get<5>(comp);
    auto compSize = std::get<6>(comp);

    if (length < PLDM_FWUP_BASELINE_TRANSFER_SIZE || length > maxTransferSize)
    {
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

    const auto& comp = compImageInfos[applicableCompOffsets[componentIndex]];
    const auto& compVersion = std::get<7>(comp);

    if (transferResult == PLDM_FWUP_TRANSFER_SUCCESS)
    {
        std::cout << "Component Transfer complete, EID=" << unsigned(eid)
                  << ", COMPONENT_VERSION=" << compVersion;
    }
    else
    {
        std::cerr << "Transfer of the component failed, EID=" << unsigned(eid)
                  << ", COMPONENT_VERSION=" << compVersion
                  << ", TRANSFER_RESULT=" << unsigned(transferResult) << "\n";
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

    const auto& comp = compImageInfos[applicableCompOffsets[componentIndex]];
    const auto& compVersion = std::get<7>(comp);

    if (verifyResult == PLDM_FWUP_VERIFY_SUCCESS)
    {
        std::cout << "Component verification complete, EID=" << unsigned(eid)
                  << ", COMPONENT_VERSION=" << compVersion;
    }
    else
    {
        std::cerr << "Component verification failed, EID=" << unsigned(eid)
                  << ", COMPONENT_VERSION=" << compVersion
                  << ", VERIFY_RESULT=" << unsigned(verifyResult) << "\n";
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

    const auto& comp = compImageInfos[applicableCompOffsets[componentIndex]];
    const auto& compVersion = std::get<7>(comp);

    if (applyResult == PLDM_FWUP_APPLY_SUCCESS)
    {
        std::cout << "Component apply complete, EID=" << unsigned(eid)
                  << ", COMPONENT_VERSION=" << compVersion;
    }
    else
    {
        std::cerr << "Component apply failed, EID=" << unsigned(eid)
                  << ", COMPONENT_VERSION=" << compVersion
                  << ", APPLY_RESULT=" << unsigned(applyResult) << "\n";
    }

    rc = encode_apply_complete_resp(request->hdr.instance_id, completionCode,
                                    responseMsg, sizeof(completionCode));
    if (rc)
    {
        std::cerr << "Encoding ApplyComplete response failed, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
        return response;
    }

    // Optional metadata and GetPackageData not handled
    if (componentIndex == applicableCompOffsets.size())
    {
        componentIndex = 0;
        pldmRequest = std::make_unique<sdeventplus::source::Defer>(
            event,
            std::bind(&DeviceUpdater::sendActivateFirmwareRequest, this));
    }
    else
    {
        componentIndex++;
        pldmRequest = std::make_unique<sdeventplus::source::Defer>(
            event, std::bind(&DeviceUpdater::sendUpdateComponentRequest, this,
                             componentIndex));
    }

    return response;
}

void DeviceUpdater::sendActivateFirmwareRequest()
{
    pldmRequest.reset();
    auto instanceId = requester.getInstanceId(eid);
    Request request(sizeof(pldm_msg_hdr) +
                    sizeof(struct pldm_activate_firmware_req));
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());

    auto rc = encode_activate_firmware_req(
        instanceId, PLDM_NOT_ACTIVATE_SELF_CONTAINED_COMPONENTS, requestMsg,
        sizeof(pldm_activate_firmware_req));
    if (rc)
    {
        requester.markFree(eid, instanceId);
        std::cerr << "encode_activate_firmware_req failed, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
    }

    rc = handler.registerRequest(
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
        // Stop any timers, log and update UpdateManager with failure
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
        std::cerr << "Decoding ActivateFirmware response failed, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
        return;
    }
    if (completionCode)
    {
        std::cerr << "ActivateFirmware response failed with error "
                     "completion code, EID="
                  << unsigned(eid) << ", CC=" << unsigned(completionCode)
                  << "\n";
        return;
    }
}

} // namespace fw_update

} // namespace pldm