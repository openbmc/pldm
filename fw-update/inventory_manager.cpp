#include "inventory_manager.hpp"

#include "common/utils.hpp"
#include "xyz/openbmc_project/Software/Version/server.hpp"

#include <libpldm/firmware_update.h>

#include <phosphor-logging/lg2.hpp>

#include <functional>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace fw_update
{
void InventoryManager::discoverFDs(const std::vector<mctp_eid_t>& eids)
{
    for (const auto& eid : eids)
    {
        try
        {
            sendQueryDeviceIdentifiersRequest(eid);
        }
        catch (const std::exception& e)
        {
            error(
                "Failed to discover file descriptors for endpoint ID {EID} with {ERROR}",
                "EID", eid, "ERROR", e);
        }
    }
}

void InventoryManager::sendQueryDeviceIdentifiersRequest(mctp_eid_t eid)
{
    auto instanceId = instanceIdDb.next(eid);
    Request requestMsg(
        sizeof(pldm_msg_hdr) + PLDM_QUERY_DEVICE_IDENTIFIERS_REQ_BYTES);
    auto request = new (requestMsg.data()) pldm_msg;
    auto rc = encode_query_device_identifiers_req(
        instanceId, PLDM_QUERY_DEVICE_IDENTIFIERS_REQ_BYTES, request);
    if (rc)
    {
        instanceIdDb.free(eid, instanceId);
        error(
            "Failed to encode query device identifiers request for endpoint ID {EID} with response code {RC}",
            "EID", eid, "RC", rc);
        throw std::runtime_error(
            "Failed to encode QueryDeviceIdentifiers request");
    }

    rc = handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_QUERY_DEVICE_IDENTIFIERS,
        std::move(requestMsg),
        [this](mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen) {
            this->queryDeviceIdentifiers(eid, response, respMsgLen);
        });
    if (rc)
    {
        error(
            "Failed to send query device identifiers request for endpoint ID {EID} with response code {RC}",
            "EID", eid, "RC", rc);
        throw std::runtime_error(
            "Failed to send QueryDeviceIdentifiers request");
    }
}

void InventoryManager::queryDeviceIdentifiers(
    mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        error(
            "No response received for query device identifiers for endpoint ID {EID}",
            "EID", eid);
        return;
    }

    uint8_t completionCode = PLDM_SUCCESS;
    uint32_t deviceIdentifiersLen = 0;
    uint8_t descriptorCount = 0;
    uint8_t* descriptorPtr = nullptr;

    auto rc = decode_query_device_identifiers_resp(
        response, respMsgLen, &completionCode, &deviceIdentifiersLen,
        &descriptorCount, &descriptorPtr);
    if (rc)
    {
        error(
            "Failed to decode query device identifiers response for endpoint ID {EID} and descriptor count {DESCRIPTOR_COUNT}, response code {RC}",
            "EID", eid, "DESCRIPTOR_COUNT", descriptorCount, "RC", rc);
        return;
    }

    if (completionCode)
    {
        error(
            "Failed to query device identifiers response for endpoint ID {EID}, completion code {CC}",
            "EID", eid, "CC", completionCode);
        return;
    }

    Descriptors descriptors{};
    while (descriptorCount-- && (deviceIdentifiersLen > 0))
    {
        uint16_t descriptorType = 0;
        variable_field descriptorData{};

        rc = decode_descriptor_type_length_value(
            descriptorPtr, deviceIdentifiersLen, &descriptorType,
            &descriptorData);
        if (rc)
        {
            error(
                "Failed to decode descriptor type {TYPE}, length {LENGTH} and value for endpoint ID {EID}, response code {RC}",
                "TYPE", descriptorType, "LENGTH", deviceIdentifiersLen, "EID",
                eid, "RC", rc);
            return;
        }

        if (descriptorType != PLDM_FWUP_VENDOR_DEFINED)
        {
            std::vector<uint8_t> descData(
                descriptorData.ptr, descriptorData.ptr + descriptorData.length);
            descriptors.emplace(descriptorType, std::move(descData));
        }
        else
        {
            uint8_t descriptorTitleStrType = 0;
            variable_field descriptorTitleStr{};
            variable_field vendorDefinedDescriptorData{};

            rc = decode_vendor_defined_descriptor_value(
                descriptorData.ptr, descriptorData.length,
                &descriptorTitleStrType, &descriptorTitleStr,
                &vendorDefinedDescriptorData);
            if (rc)
            {
                error(
                    "Failed to decode vendor-defined descriptor value for endpoint ID {EID}, response code {RC}",
                    "EID", eid, "RC", rc);
                return;
            }

            auto vendorDefinedDescriptorTitleStr =
                utils::toString(descriptorTitleStr);
            std::vector<uint8_t> vendorDescData(
                vendorDefinedDescriptorData.ptr,
                vendorDefinedDescriptorData.ptr +
                    vendorDefinedDescriptorData.length);
            descriptors.emplace(descriptorType,
                                std::make_tuple(vendorDefinedDescriptorTitleStr,
                                                vendorDescData));
        }
        auto nextDescriptorOffset =
            sizeof(pldm_descriptor_tlv().descriptor_type) +
            sizeof(pldm_descriptor_tlv().descriptor_length) +
            descriptorData.length;
        descriptorPtr += nextDescriptorOffset;
        deviceIdentifiersLen -= nextDescriptorOffset;
    }

    descriptorMap.emplace(eid, std::move(descriptors));

    // Send GetFirmwareParameters request
    sendGetFirmwareParametersRequest(eid);
}

void InventoryManager::sendQueryDownstreamDevicesRequest(mctp_eid_t eid)
{
    Request requestMsg(sizeof(pldm_msg_hdr));
    auto instanceId = instanceIdDb.next(eid);
    auto request = new (requestMsg.data()) pldm_msg;
    auto rc = encode_query_downstream_devices_req(instanceId, request);
    if (rc)
    {
        instanceIdDb.free(eid, instanceId);
        error(
            "Failed to encode query downstream devices request for endpoint ID EID {EID} with response code {RC}",
            "EID", eid, "RC", rc);
        throw std::runtime_error(
            "Failed to encode query downstream devices request");
    }

    rc = handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_QUERY_DOWNSTREAM_DEVICES,
        std::move(requestMsg),
        [this](mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen) {
            this->queryDownstreamDevices(eid, response, respMsgLen);
        });
    if (rc)
    {
        error(
            "Failed to send QueryDownstreamDevices request for endpoint ID {EID} with response code {RC}",
            "EID", eid, "RC", rc);
    }
}

void InventoryManager::queryDownstreamDevices(
    mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen)
{
    if (!response || !respMsgLen)
    {
        error(
            "No response received for QueryDownstreamDevices for endpoint ID {EID}",
            "EID", eid);
        return;
    }

    pldm_query_downstream_devices_resp downstreamDevicesResp{};
    auto rc = decode_query_downstream_devices_resp(response, respMsgLen,
                                                   &downstreamDevicesResp);
    if (rc)
    {
        error(
            "Decoding QueryDownstreamDevices response failed for endpoint ID {EID} with response code {RC}",
            "EID", eid, "RC", rc);
        return;
    }

    switch (downstreamDevicesResp.completion_code)
    {
        case PLDM_SUCCESS:
            break;
        case PLDM_ERROR_UNSUPPORTED_PLDM_CMD:
            /* QueryDownstreamDevices is optional, consider the device does not
             * support Downstream Devices.
             */
            info("Endpoint ID {EID} does not support QueryDownstreamDevices",
                 "EID", eid);
            return;
        default:
            error(
                "QueryDownstreamDevices response failed with error completion code for endpoint ID {EID} with completion code {CC}",
                "EID", eid, "CC", downstreamDevicesResp.completion_code);
            return;
    }

    switch (downstreamDevicesResp.downstream_device_update_supported)
    {
        case PLDM_FWUP_DOWNSTREAM_DEVICE_UPDATE_SUPPORTED:
            /** DataTransferHandle will be skipped when TransferOperationFlag is
             *  `GetFirstPart`. Use 0x0 as default by following example in
             *  Figure 9 in DSP0267 1.1.0
             */
            try
            {
                sendQueryDownstreamIdentifiersRequest(eid, 0x0,
                                                      PLDM_GET_FIRSTPART);
            }
            catch (const std::exception& e)
            {
                error(
                    "Failed to send QueryDownstreamIdentifiers request for endpoint ID {EID} with {ERROR}",
                    "EID", eid, "ERROR", e);
            }
            break;
        case PLDM_FWUP_DOWNSTREAM_DEVICE_UPDATE_NOT_SUPPORTED:
            /* The FDP does not support firmware updates but may report
             * inventory information on downstream devices.
             * In this scenario, sends only GetDownstreamFirmwareParameters
             * to the FDP.
             * The definition can be found at Table 15 of DSP0267_1.1.0
             */
            break;
        default:
            error(
                "Unknown response of DownstreamDeviceUpdateSupported from endpoint ID {EID} with value {VALUE}",
                "EID", eid, "VALUE",
                downstreamDevicesResp.downstream_device_update_supported);
            return;
    }
}

void InventoryManager::sendQueryDownstreamIdentifiersRequest(
    mctp_eid_t eid, uint32_t dataTransferHandle,
    enum transfer_op_flag transferOperationFlag)
{
    auto instanceId = instanceIdDb.next(eid);
    Request requestMsg(
        sizeof(pldm_msg_hdr) + PLDM_QUERY_DOWNSTREAM_IDENTIFIERS_REQ_BYTES);
    auto request = new (requestMsg.data()) pldm_msg;
    pldm_query_downstream_identifiers_req requestParameters{
        dataTransferHandle, static_cast<uint8_t>(transferOperationFlag)};

    auto rc = encode_query_downstream_identifiers_req(
        instanceId, &requestParameters, request,
        PLDM_QUERY_DOWNSTREAM_IDENTIFIERS_REQ_BYTES);
    if (rc)
    {
        instanceIdDb.free(eid, instanceId);
        error(
            "Failed to encode query downstream identifiers request for endpoint ID {EID} with response code {RC}",
            "EID", eid, "RC", rc);
        throw std::runtime_error(
            "Failed to encode query downstream identifiers request");
    }

    rc = handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_QUERY_DOWNSTREAM_IDENTIFIERS,
        std::move(requestMsg),
        [this](mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen) {
            this->queryDownstreamIdentifiers(eid, response, respMsgLen);
        });
    if (rc)
    {
        error(
            "Failed to send QueryDownstreamIdentifiers request for endpoint ID {EID} with response code {RC}",
            "EID", eid, "RC", rc);
    }
}

void InventoryManager::queryDownstreamIdentifiers(
    mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen)
{
    if (!response || !respMsgLen)
    {
        error(
            "No response received for QueryDownstreamIdentifiers for endpoint ID {EID}",
            "EID", eid);
        descriptorMap.erase(eid);
        return;
    }

    pldm_query_downstream_identifiers_resp downstreamIds{};
    pldm_downstream_device_iter devs{};

    auto rc = decode_query_downstream_identifiers_resp(response, respMsgLen,
                                                       &downstreamIds, &devs);
    if (rc)
    {
        error(
            "Decoding QueryDownstreamIdentifiers response failed for endpoint ID {EID} with response code {RC}",
            "EID", eid, "RC", rc);
        return;
    }

    if (downstreamIds.completion_code)
    {
        error(
            "QueryDownstreamIdentifiers response failed with error completion code for endpoint ID {EID} with completion code {CC}",
            "EID", eid, "CC", unsigned(downstreamIds.completion_code));
        return;
    }

    DownstreamDeviceInfo initialDownstreamDevices{};
    DownstreamDeviceInfo* downstreamDevices;
    if (!downstreamDescriptorMap.contains(eid) ||
        downstreamIds.transfer_flag == PLDM_START ||
        downstreamIds.transfer_flag == PLDM_START_AND_END)
    {
        downstreamDevices = &initialDownstreamDevices;
    }
    else
    {
        downstreamDevices = &downstreamDescriptorMap.at(eid);
    }

    pldm_downstream_device dev;
    foreach_pldm_downstream_device(devs, dev, rc)
    {
        pldm_descriptor desc;
        Descriptors descriptors{};
        foreach_pldm_downstream_device_descriptor(devs, dev, desc, rc)
        {
            const auto descriptorData =
                new (const_cast<void*>(desc.descriptor_data))
                    uint8_t[desc.descriptor_length];
            if (desc.descriptor_type != PLDM_FWUP_VENDOR_DEFINED)
            {
                std::vector<uint8_t> descData(
                    descriptorData, descriptorData + desc.descriptor_length);
                descriptors.emplace(desc.descriptor_type, std::move(descData));
            }
            else
            {
                uint8_t descriptorTitleStrType = 0;
                variable_field descriptorTitleStr{};
                variable_field vendorDefinedDescriptorData{};

                rc = decode_vendor_defined_descriptor_value(
                    descriptorData, desc.descriptor_length,
                    &descriptorTitleStrType, &descriptorTitleStr,
                    &vendorDefinedDescriptorData);

                if (rc)
                {
                    error(
                        "Decoding Vendor-defined descriptor value failed for endpoint ID {EID} with response code {RC}",
                        "EID", eid, "RC", rc);
                    return;
                }

                auto vendorDefinedDescriptorTitleStr =
                    utils::toString(descriptorTitleStr);
                std::vector<uint8_t> vendorDescData(
                    vendorDefinedDescriptorData.ptr,
                    vendorDefinedDescriptorData.ptr +
                        vendorDefinedDescriptorData.length);
                descriptors.emplace(
                    desc.descriptor_type,
                    std::make_tuple(vendorDefinedDescriptorTitleStr,
                                    vendorDescData));
            }
        }
        if (rc)
        {
            error(
                "Failed to decode downstream device descriptor for endpoint ID {EID} with response code {RC}",
                "EID", eid, "RC", rc);
            return;
        }
        downstreamDevices->emplace(dev.downstream_device_index, descriptors);
    }
    if (rc)
    {
        error(
            "Failed to decode downstream devices from iterator for endpoint ID {EID} with response code {RC}",
            "EID", eid, "RC", rc);
        return;
    }

    switch (downstreamIds.transfer_flag)
    {
        case PLDM_START:
            downstreamDescriptorMap.insert_or_assign(
                eid, std::move(initialDownstreamDevices));
            [[fallthrough]];
        case PLDM_MIDDLE:
            sendQueryDownstreamIdentifiersRequest(
                eid, downstreamIds.next_data_transfer_handle,
                PLDM_GET_NEXTPART);
            break;
        case PLDM_START_AND_END:
            downstreamDescriptorMap.insert_or_assign(
                eid, std::move(initialDownstreamDevices));
            /** DataTransferHandle will be skipped when TransferOperationFlag is
             *  `GetFirstPart`. Use 0x0 as default by following example in
             *  Figure 9 in DSP0267 1.1.0
             */
            [[fallthrough]];
        case PLDM_END:
            sendGetDownstreamFirmwareParametersRequest(eid, 0x0,
                                                       PLDM_GET_FIRSTPART);
            break;
    }
}

void InventoryManager::sendGetDownstreamFirmwareParametersRequest(
    mctp_eid_t eid, uint32_t dataTransferHandle,
    enum transfer_op_flag transferOperationFlag)
{
    Request requestMsg(sizeof(pldm_msg_hdr) +
                       PLDM_GET_DOWNSTREAM_FIRMWARE_PARAMETERS_REQ_BYTES);
    auto instanceId = instanceIdDb.next(eid);
    auto request = new (requestMsg.data()) pldm_msg;
    pldm_get_downstream_firmware_parameters_req requestParameters{
        dataTransferHandle, static_cast<uint8_t>(transferOperationFlag)};
    auto rc = encode_get_downstream_firmware_parameters_req(
        instanceId, &requestParameters, request,
        PLDM_GET_DOWNSTREAM_FIRMWARE_PARAMETERS_REQ_BYTES);
    if (rc)
    {
        instanceIdDb.free(eid, instanceId);
        error(
            "Failed to encode query downstream firmware parameters request for endpoint ID {EID} with response code {RC}",
            "EID", eid, "RC", rc);
        throw std::runtime_error(
            "Failed to encode query downstream firmware parameters request");
    }

    rc = handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_QUERY_DOWNSTREAM_FIRMWARE_PARAMETERS,
        std::move(requestMsg),
        [this](mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen) {
            this->getDownstreamFirmwareParameters(eid, response, respMsgLen);
        });
    if (rc)
    {
        error(
            "Failed to send QueryDownstreamFirmwareParameters request for endpoint ID {EID} with response code {RC}",
            "EID", eid, "RC", rc);
    }
}

void InventoryManager::getDownstreamFirmwareParameters(
    mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen)
{
    if (!response || !respMsgLen)
    {
        error(
            "No response received for QueryDownstreamFirmwareParameters for endpoint ID {EID}",
            "EID", eid);
        descriptorMap.erase(eid);
        return;
    }

    pldm_get_downstream_firmware_parameters_resp resp{};
    pldm_downstream_device_parameters_iter params{};
    pldm_downstream_device_parameters_entry entry{};

    auto rc = decode_get_downstream_firmware_parameters_resp(
        response, respMsgLen, &resp, &params);

    if (rc)
    {
        error(
            "Decoding QueryDownstreamFirmwareParameters response failed for endpoint ID {EID} with response code {RC}",
            "EID", eid, "RC", rc);
        return;
    }

    if (resp.completion_code)
    {
        error(
            "QueryDownstreamFirmwareParameters response failed with error completion code for endpoint ID {EID} with completion code {CC}",
            "EID", eid, "CC", resp.completion_code);
        return;
    }

    foreach_pldm_downstream_device_parameters_entry(params, entry, rc)
    {
        // Reserved for upcoming use
        [[maybe_unused]] variable_field activeCompVerStr{
            reinterpret_cast<const uint8_t*>(entry.active_comp_ver_str),
            entry.active_comp_ver_str_len};
    }
    if (rc)
    {
        error(
            "Failed to decode downstream device parameters from iterator for endpoint ID {EID} with response code {RC}",
            "EID", eid, "RC", rc);
        return;
    }

    switch (resp.transfer_flag)
    {
        case PLDM_START:
        case PLDM_MIDDLE:
            sendGetDownstreamFirmwareParametersRequest(
                eid, resp.next_data_transfer_handle, PLDM_GET_NEXTPART);
            break;
    }
}

void InventoryManager::sendGetFirmwareParametersRequest(mctp_eid_t eid)
{
    auto instanceId = instanceIdDb.next(eid);
    Request requestMsg(
        sizeof(pldm_msg_hdr) + PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES);
    auto request = new (requestMsg.data()) pldm_msg;
    auto rc = encode_get_firmware_parameters_req(
        instanceId, PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES, request);
    if (rc)
    {
        instanceIdDb.free(eid, instanceId);
        error(
            "Failed to encode get firmware parameters req for endpoint ID {EID}, response code {RC}",
            "EID", eid, "RC", rc);
        return;
    }

    rc = handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_GET_FIRMWARE_PARAMETERS,
        std::move(requestMsg),
        [this](mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen) {
            this->getFirmwareParameters(eid, response, respMsgLen);
        });
    if (rc)
    {
        error(
            "Failed to send get firmware parameters request for endpoint ID {EID}, response code {RC}",
            "EID", eid, "RC", rc);
    }
}

void InventoryManager::getFirmwareParameters(
    mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        error(
            "No response received for get firmware parameters for endpoint ID {EID}",
            "EID", eid);
        descriptorMap.erase(eid);
        return;
    }

    pldm_get_firmware_parameters_resp fwParams{};
    variable_field activeCompImageSetVerStr{};
    variable_field pendingCompImageSetVerStr{};
    variable_field compParamTable{};

    auto rc = decode_get_firmware_parameters_resp(
        response, respMsgLen, &fwParams, &activeCompImageSetVerStr,
        &pendingCompImageSetVerStr, &compParamTable);
    if (rc)
    {
        error(
            "Failed to decode get firmware parameters response for endpoint ID {EID}, response code {RC}",
            "EID", eid, "RC", rc);
        return;
    }

    if (fwParams.completion_code)
    {
        auto fw_param_cc = fwParams.completion_code;
        error(
            "Failed to get firmware parameters response for endpoint ID {EID}, completion code {CC}",
            "EID", eid, "CC", fw_param_cc);
        return;
    }

    auto compParamPtr = compParamTable.ptr;
    auto compParamTableLen = compParamTable.length;
    pldm_component_parameter_entry compEntry{};
    variable_field activeCompVerStr{};
    variable_field pendingCompVerStr{};

    ComponentInfo componentInfo{};
    while (fwParams.comp_count-- && (compParamTableLen > 0))
    {
        auto rc = decode_get_firmware_parameters_resp_comp_entry(
            compParamPtr, compParamTableLen, &compEntry, &activeCompVerStr,
            &pendingCompVerStr);
        if (rc)
        {
            error(
                "Failed to decode component parameter table entry for endpoint ID {EID}, response code {RC}",
                "EID", eid, "RC", rc);
            return;
        }

        auto compClassification = compEntry.comp_classification;
        auto compIdentifier = compEntry.comp_identifier;
        componentInfo.emplace(
            std::make_pair(compClassification, compIdentifier),
            compEntry.comp_classification_index);
        compParamPtr += sizeof(pldm_component_parameter_entry) +
                        activeCompVerStr.length + pendingCompVerStr.length;
        compParamTableLen -= sizeof(pldm_component_parameter_entry) +
                             activeCompVerStr.length + pendingCompVerStr.length;
    }
    componentInfoMap.emplace(eid, std::move(componentInfo));
}

} // namespace fw_update

} // namespace pldm
