#include "inventory_manager.hpp"

#include "common/utils.hpp"
#include "xyz/openbmc_project/Software/Version/server.hpp"

#include <libpldm/firmware_update.h>

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <functional>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace fw_update
{

#define EID_INDEX 0
#define INVENTORY_PATH_INDEX 4

void InventoryManager::discoverFDs(const MctpInfos& mctpInfos)
{
    for (const auto& mctpInfo : mctpInfos)
    {
        auto eid = std::get<EID_INDEX>(mctpInfo);
        try
        {
            refreshInventoryPath(mctpInfo);
            sendQueryDeviceIdentifiersRequest(eid);
            sendQueryDownstreamDevicesRequest(eid);
        }
        catch (const std::exception& e)
        {
            error("Failed to discover FDs, EID={EID}, Error={ERROR}", "EID",
                  unsigned(eid), "ERROR", e.what());
        }
    }
}

void InventoryManager::refreshInventoryPath(const MctpInfo& mctpInfo)
{
    const auto eid = std::get<EID_INDEX>(mctpInfo);
    const std::map<std::string, MctpEndpoint>& configurations =
        configurationDiscovery->getConfigurations();

    for (const auto& [configDbusPath, mctpEndpoint] : configurations)
    {
        if (eid == mctpEndpoint.EndpointId)
        {
            inventoryItemManager.refreshInventoryPath(eid, configDbusPath);
            break;
        }
    }
}

void InventoryManager::sendQueryDeviceIdentifiersRequest(mctp_eid_t eid)
{
    auto instanceId = instanceIdDb.next(eid);
    Request requestMsg(sizeof(pldm_msg_hdr) +
                       PLDM_QUERY_DEVICE_IDENTIFIERS_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_query_device_identifiers_req(
        instanceId, PLDM_QUERY_DEVICE_IDENTIFIERS_REQ_BYTES, request);
    if (rc)
    {
        instanceIdDb.free(eid, instanceId);
        error(
            "encode_query_device_identifiers_req failed, EID={EID}, RC = {RC}",
            "EID", unsigned(eid), "RC", rc);
        throw std::runtime_error(
            "Failed to encode QueryDeviceIdentifiers request");
    }

    rc = handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_QUERY_DEVICE_IDENTIFIERS,
        std::move(requestMsg),
        std::move(
            std::bind_front(&InventoryManager::queryDeviceIdentifiers, this)));
    if (rc)
    {
        error(
            "Failed to send query device identifiers request for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
        throw std::runtime_error(
            "Failed to send QueryDeviceIdentifiers request");
    }
}

void InventoryManager::queryDeviceIdentifiers(mctp_eid_t eid,
                                              const pldm_msg* response,
                                              size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        error(
            "No response received for query device identifiers for endpoint ID '{EID}'",
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
            "Failed to decode query device identifiers response for endpoint ID '{EID}' and descriptor count '{DESCRIPTOR_COUNT}', response code '{RC}'",
            "EID", eid, "DESCRIPTOR_COUNT", descriptorCount, "RC", rc);
        return;
    }

    if (completionCode)
    {
        error(
            "Failed to query device identifiers response for endpoint ID '{EID}', completion code '{CC}'",
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
                "Failed to decode descriptor type {TYPE}, length {LENGTH} and value for endpoint ID '{EID}', response code '{RC}'",
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
                    "Failed to decode vendor-defined descriptor value for endpoint ID '{EID}', response code '{RC}'",
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

    updateFirmwareDeviceName(eid, descriptors);
    // Send GetFirmwareParameters request
    sendGetFirmwareParametersRequest(eid);
}

void InventoryManager::sendQueryDownstreamDevicesRequest(mctp_eid_t eid)
{
    Request requestMsg(sizeof(pldm_msg_hdr));
    auto instanceId = instanceIdDb.next(eid);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_query_downstream_devices_req(instanceId, request);
    if (rc)
    {
        instanceIdDb.free(eid, instanceId);
        error(
            "encode_query_downstream_devices_req failed, EID={EID}, RC = {RC}",
            "EID", unsigned(eid), "RC", rc);
        throw std::runtime_error("encode_query_device_identifiers_req failed");
    }

    rc = handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_QUERY_DOWNSTREAM_DEVICES,
        std::move(requestMsg),
        std::move(
            std::bind_front(&InventoryManager::queryDownstreamDevices, this)));
    if (rc)
    {
        error(
            "Failed to send QueryDownstreamDevices request, EID={EID}, RC = {RC}",
            "EID", unsigned(eid), "RC", rc);
    }
}

void InventoryManager::queryDownstreamDevices(mctp_eid_t eid,
                                              const pldm_msg* response,
                                              size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        error("No response received for QueryDownstreamDevices, EID={EID}",
              "EID", unsigned(eid));
        return;
    }

    pldm_query_downstream_devices_resp downstreamDevicesResp{};
    auto rc = decode_query_downstream_devices_resp(response, respMsgLen,
                                                   &downstreamDevicesResp);
    if (rc)
    {
        error(
            "Decoding QueryDownstreamDevices response failed, EID={EID}, RC = {RC}",
            "EID", unsigned(eid), "RC", rc);
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
            return;
        default:
            error(
                "QueryDownstreamDevices response failed with error completion code, EID={EID}, CC = {CC}",
                "EID", unsigned(eid), "CC",
                unsigned(downstreamDevicesResp.completion_code));
            return;
    }

    switch (downstreamDevicesResp.downstream_device_update_supported)
    {
        case PLDM_FWUP_DOWNSTREAM_DEVICE_UPDATE_SUPPORTED:
            /** DataTransferHandle will be skipped when TransferOperationFlag is
             *  `GetFirstPart`. Use 0x0 as default by following example in
             *  Figure 9 in DSP0267 1.1.0
             */
            sendQueryDownstreamIdentifiersRequest(eid, 0x0, PLDM_GET_FIRSTPART);
            break;
        case PLDM_FWUP_DOWNSTREAM_DEVICE_UPDATE_NOT_SUPPORTED:
            /* The FDP does not support firmware updates but may report
             * inventory information on downstream devices.
             * In this scenario, sends only GetDownstreamFirmwareParameters
             * to the FDP.
             * The difinition can be found at Table 15 of DSP0267_1.1.0
             */
            break;
        default:
            error(
                "Unknown response of DownstreamDeviceUpdateSupported from EID={EID}, Value = {VALUE}",
                "EID", unsigned(eid), "VALUE",
                unsigned(
                    downstreamDevicesResp.downstream_device_update_supported));
            return;
    }
}

void InventoryManager::sendQueryDownstreamIdentifiersRequest(
    mctp_eid_t eid, uint32_t dataTransferHandle,
    enum transfer_op_flag transferOperationFlag)
{
    auto instanceId = instanceIdDb.next(eid);
    Request requestMsg(sizeof(pldm_msg_hdr) +
                       PLDM_QUERY_DOWNSTREAM_IDENTIFIERS_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = encode_query_downstream_identifiers_req(
        instanceId, dataTransferHandle, transferOperationFlag, request,
        PLDM_QUERY_DOWNSTREAM_IDENTIFIERS_REQ_BYTES);
    if (rc)
    {
        instanceIdDb.free(eid, instanceId);
        error(
            "encode_query_downstream_identifiers_req failed, EID={EID}, RC = {RC}",
            "EID", unsigned(eid), "RC", rc);
        throw std::runtime_error("encode_query_device_identifiers_req failed");
    }

    rc = handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_QUERY_DOWNSTREAM_IDENTIFIERS,
        std::move(requestMsg),
        std::move(std::bind_front(&InventoryManager::queryDownstreamIdentifiers,
                                  this)));
    if (rc)
    {
        error(
            "Failed to send QueryDownstreamIdentifiers request, EID={EID}, RC = {RC}",
            "EID", unsigned(eid), "RC", rc);
    }
}

void InventoryManager::queryDownstreamIdentifiers(mctp_eid_t eid,
                                                  const pldm_msg* response,
                                                  size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        error("No response received for QueryDownstreamIdentifiers, EID={EID}",
              "EID", unsigned(eid));
        descriptorMap.erase(eid);
        return;
    }

    pldm_query_downstream_identifiers_resp downstreamIds{};
    variable_field downstreamDevicesData{};

    auto rc = decode_query_downstream_identifiers_resp(
        response, respMsgLen, &downstreamIds, &downstreamDevicesData);
    if (rc)
    {
        error(
            "Decoding QueryDownstreamIdentifiers response failed, EID={EID}, RC = {RC}",
            "EID", unsigned(eid), "RC", rc);
        return;
    }

    if (downstreamIds.completion_code)
    {
        error(
            "QueryDownstreamIdentifiers response failed with error completion code, EID={EID}, CC = {CC}",
            "EID", unsigned(eid), "CC",
            unsigned(downstreamIds.completion_code));
        return;
    }

    DownstreamDevices downstreamDevices{};
    switch (downstreamIds.transfer_flag)
    {
        case PLDM_MIDDLE:
        case PLDM_END:
            downstreamDevices = downstreamDescriptorMap.at(eid);
            break;
    }

    while (downstreamIds.number_of_downstream_devices-- &&
           (downstreamDevicesData.length > 0))
    {
        auto downstreamDevice = reinterpret_cast<const pldm_downstream_device*>(
            downstreamDevicesData.ptr);
        auto downstreamDeviceIndex = downstreamDevice->downstream_device_index;
        auto descriptorCount = downstreamDevice->downstream_descriptor_count;
        auto descriptorPtr = downstreamDevicesData.ptr +
                             PLDM_DOWNSTREAM_DEVICE_BYTES;

        Descriptors descriptors{};
        for (uint8_t i = 0; i < descriptorCount; i++)
        {
            uint16_t descriptorType = 0;
            variable_field descriptorData{};

            rc = decode_descriptor_type_length_value(
                descriptorPtr, downstreamDevicesData.length, &descriptorType,
                &descriptorData);

            if (rc)
            {
                error(
                    "Decoding downstream descriptor type, length and value failed, EID={EID}, RC = {RC}",
                    "EID", unsigned(eid), "RC", rc);
                return;
            }

            if (descriptorType != PLDM_FWUP_VENDOR_DEFINED)
            {
                std::vector<uint8_t> descData(descriptorData.ptr,
                                              descriptorData.ptr +
                                                  descriptorData.length);
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
                        "Decoding Vendor-defined descriptor value failed, EID={EID}, RC = {RC}",
                        "EID", unsigned(eid), "RC", rc);
                    return;
                }

                auto vendorDefinedDescriptorTitleStr =
                    utils::toString(descriptorTitleStr);
                std::vector<uint8_t> vendorDescData(
                    vendorDefinedDescriptorData.ptr,
                    vendorDefinedDescriptorData.ptr +
                        vendorDefinedDescriptorData.length);
                descriptors.emplace(
                    descriptorType,
                    std::make_tuple(vendorDefinedDescriptorTitleStr,
                                    vendorDescData));
            }
            auto nextDescriptorOffset =
                sizeof(pldm_descriptor_tlv().descriptor_type) +
                sizeof(pldm_descriptor_tlv().descriptor_length) +
                descriptorData.length;
            descriptorPtr += nextDescriptorOffset;
            downstreamDevicesData.length -= nextDescriptorOffset;
        }

        downstreamDevicesData.length -= PLDM_DOWNSTREAM_DEVICE_BYTES;
        downstreamDevicesData.ptr = descriptorPtr;
        downstreamDevices.emplace_back(
            DownstreamDeviceInfo{downstreamDeviceIndex, descriptors});

        updateDownstreamDeviceName(eid, downstreamDeviceIndex, descriptors);
    }

    switch (downstreamIds.transfer_flag)
    {
        case PLDM_START:
            downstreamDescriptorMap.emplace(eid, std::move(downstreamDevices));
            sendQueryDownstreamIdentifiersRequest(
                eid, downstreamIds.next_data_transfer_handle,
                PLDM_GET_NEXTPART);
            break;
        case PLDM_MIDDLE:
            sendQueryDownstreamIdentifiersRequest(
                eid, downstreamIds.next_data_transfer_handle,
                PLDM_GET_NEXTPART);
            break;
        case PLDM_START_AND_END:
            downstreamDescriptorMap.emplace(eid, std::move(downstreamDevices));
            /** DataTransferHandle will be skipped when TransferOperationFlag is
             *  `GetFirstPart`. Use 0x0 as default by following example in
             *  Figure 9 in DSP0267 1.1.0
             */
            sendGetDownstreamFirmwareParametersRequest(eid, 0x0,
                                                       PLDM_GET_FIRSTPART);
            break;
        case PLDM_END:
            sendGetDownstreamFirmwareParametersRequest(eid, 0x0,
                                                       PLDM_GET_FIRSTPART);
            break;
    }
}

void InventoryManager::sendGetDownstreamFirmwareParametersRequest(
    mctp_eid_t eid, uint32_t dataTransferHandle,
    const enum transfer_op_flag transferOperationFlag)
{
    Request requestMsg(sizeof(pldm_msg_hdr) +
                       PLDM_QUERY_DOWNSTREAM_IDENTIFIERS_REQ_BYTES);
    auto instanceId = instanceIdDb.next(eid);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_get_downstream_firmware_params_req(
        instanceId, dataTransferHandle, transferOperationFlag,
        request, PLDM_QUERY_DOWNSTREAM_IDENTIFIERS_REQ_BYTES);
    if (rc)
    {
        instanceIdDb.free(eid, instanceId);
        error(
            "encode_query_downstream_firmware_param_req failed, EID={EID}, RC = {RC}",
            "EID", unsigned(eid), "RC", rc);
        throw std::runtime_error(
            "encode_query_device_firmware_param_req failed");
    }

    rc = handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_QUERY_DOWNSTREAM_FIRMWARE_PARAMETERS,
        std::move(requestMsg),
        std::move(std::bind_front(
            &InventoryManager::getDownstreamFirmwareParameters, this)));
    if (rc)
    {
        error(
            "Failed to send QueryDownstreamFirmwareParameters request, EID={EID}, RC = {RC}",
            "EID", unsigned(eid), "RC", rc);
    }
}

void InventoryManager::updateDownstreamDeviceName(
    const eid& eid, const DownstreamDeviceIndex& downstreamDeviceIndex,
    const Descriptors& descriptors)
{
    FirmwareDeviceName deviceName =
        obtainDeviceNameFromDescriptors(descriptors);

    if (deviceName.empty())
    {
        deviceName = "Downstream_Device_" +
                     std::to_string(downstreamDeviceIndex);
    }

    downstreamDeviceNameMap.emplace(std::make_tuple(eid, downstreamDeviceIndex),
                                    deviceName);
}

FirmwareDeviceName InventoryManager::obtainDeviceNameFromDescriptors(
    const Descriptors& descriptors)
{
    FirmwareDeviceName deviceName{};
    for (const auto& [descriptorType, descriptorData] : descriptors)
    {
        if (descriptorType == PLDM_FWUP_ASCII_MODEL_NUMBER_LONG_STRING)
        {
            auto modelNumberData = std::get<DescriptorData>(descriptorData);
            std::string modelNumber(
                modelNumberData.begin(),
                std::find(modelNumberData.begin(), modelNumberData.end(), 0x0));
            if (deviceName.empty())
            {
                deviceName = modelNumber;
            }
            else
            {
                deviceName.insert(0, modelNumber + "_");
            }
        }
        else if (descriptorType == PLDM_FWUP_ASCII_MODEL_NUMBER_SHORT_STRING)
        {
            auto modelNumberData = std::get<DescriptorData>(descriptorData);
            std::string modelNumber(
                modelNumberData.begin(),
                std::find(modelNumberData.begin(), modelNumberData.end(), 0x0));
            if (deviceName.empty())
            {
                deviceName = modelNumber;
            }
            else
            {
                deviceName += "_" + modelNumber;
            }
        }
    }

    return deviceName;
}

void InventoryManager::updateFirmwareDeviceName(const eid& eid,
                                                const Descriptors& descriptors)
{
    auto config = configurationDiscovery->getConfigurations();
    FirmwareDeviceName firmwareDeviceName;
    for (const auto& [configDbusPath, mctpEndpoint] : config)
    {
        if (mctpEndpoint.EndpointId == eid)
        {
            firmwareDeviceName =
                configDbusPath.substr(configDbusPath.find_last_of('/') + 1);
        }
    }

    if (firmwareDeviceName.empty())
    {
        firmwareDeviceName = obtainDeviceNameFromDescriptors(descriptors);
    }

    if (firmwareDeviceName.empty())
    {
        firmwareDeviceName = "Firmware_Device_" + std::to_string(eid);
    }

    firmwareDeviceNameMap.emplace(eid, firmwareDeviceName);
}

void InventoryManager::getDownstreamFirmwareParameters(mctp_eid_t eid,
                                                       const pldm_msg* response,
                                                       size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        error(
            "No response received for QueryDownstreamFirmwareParameters, EID={EID}",
            "EID", unsigned(eid));
        descriptorMap.erase(eid);
        return;
    }

    pldm_get_downstream_firmware_params_resp downstreamFirmwareParams{};
    variable_field downstreamDeviceParamTable{};

    auto rc = decode_get_downstream_firmware_params_resp(
        response, respMsgLen, &downstreamFirmwareParams,
        &downstreamDeviceParamTable);

    if (rc)
    {
        error(
            "Decoding QueryDownstreamFirmwareParameters response failed, EID={EID}, RC = {RC}",
            "EID", unsigned(eid), "RC", rc);
        return;
    }

    if (downstreamFirmwareParams.completion_code)
    {
        error(
            "QueryDownstreamFirmwareParameters response failed with error completion code, EID={EID}, CC = {CC}",
            "EID", unsigned(eid), "CC",
            unsigned(downstreamFirmwareParams.completion_code));
        return;
    }

    struct pldm_downstream_device_parameter_entry_versions entry_version = {};
    struct variable_field versions = {};
    ComponentInfo componentInfo{};

    while (downstreamFirmwareParams.downstream_device_count-- &&
           (downstreamDeviceParamTable.length > 0))
    {
        auto rc = decode_downstream_device_parameter_table_entry(
            &downstreamDeviceParamTable, &entry_version.entry, &versions);
        if (rc)
        {
            error(
                "Decoding downstream device parameter table entry failed, EID={EID}, RC = {RC}",
                "EID", unsigned(eid), "RC", rc);
            return;
        }

        rc = decode_downstream_device_parameter_table_entry_versions(
            &versions, &entry_version.entry, entry_version.active_comp_ver_str,
            entry_version.pending_comp_ver_str);
        if (rc)
        {
            error(
                "Decoding downstream device parameter table entry versions failed, EID={EID}, RC = {RC}",
                "EID", unsigned(eid), "RC", rc);
            return;
        }

        // Classification for downstream devices defined in DSP0267_1.1.0 Table
        // 27 – ComponentClassification values
        auto compClassification = 0xFFFF;
        auto deviceIndex = entry_version.entry.downstream_device_index;
        // Update only the downsltream device which match the target
        // deviceIndex, 0x0 is defined in DSP0267_1.1.0 Table 25 –
        // PassComponentTable command format
        auto componentClassificationIndex = 0x0;
        componentInfo.emplace(std::make_pair(compClassification, deviceIndex),
                              componentClassificationIndex);

        const std::string activeCompVerStr =
            entry_version.entry.active_comp_ver_str;
        inventoryItemManager.createInventoryItem(
            eid,
            downstreamDeviceNameMap.at(std::make_tuple(eid, compIdentifier)),
            activeCompVerStr);
    }

    switch (downstreamFirmwareParams.transfer_flag)
    {
        case PLDM_START:
        case PLDM_MIDDLE:
            sendGetDownstreamFirmwareParametersRequest(
                eid, downstreamFirmwareParams.next_data_transfer_handle,
                PLDM_GET_NEXTPART);
            break;
    }

    if (componentInfoMap.contains(eid))
    {
        componentInfoMap.at(eid).merge(componentInfo);
    }
    else
    {
        componentInfoMap.emplace(eid, std::move(componentInfo));
    }
}

void InventoryManager::sendGetFirmwareParametersRequest(mctp_eid_t eid)
{
    auto instanceId = instanceIdDb.next(eid);
    Request requestMsg(sizeof(pldm_msg_hdr) +
                       PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_get_firmware_parameters_req(
        instanceId, PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES, request);
    if (rc)
    {
        instanceIdDb.free(eid, instanceId);
        error(
            "Failed to encode get firmware parameters req for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
        return;
    }

    rc = handler.registerRequest(
        eid, instanceId, PLDM_FWUP, PLDM_GET_FIRMWARE_PARAMETERS,
        std::move(requestMsg),
        std::bind_front(&InventoryManager::getFirmwareParameters, this));
    if (rc)
    {
        error(
            "Failed to send get firmware parameters request for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
    }
}

void InventoryManager::getFirmwareParameters(mctp_eid_t eid,
                                             const pldm_msg* response,
                                             size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        error(
            "No response received for get firmware parameters for endpoint ID '{EID}'",
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
            "Failed to decode get firmware parameters response for endpoint ID '{EID}', response code '{RC}'",
            "EID", eid, "RC", rc);
        return;
    }

    if (fwParams.completion_code)
    {
        auto fw_param_cc = fwParams.completion_code;
        error(
            "Failed to get firmware parameters response for endpoint ID '{EID}', completion code '{CC}'",
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
                "Failed to decode component parameter table entry for endpoint ID '{EID}', response code '{RC}'",
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

    descriptorMaps.emplace_back(std::make_unique<DescriptorMap>(DescriptorMap{{eid, descriptorMap.at(eid)}}));
    componentInfoMaps.emplace_back(std::make_unique<ComponentInfoMap>(ComponentInfoMap{{eid, componentInfoMap.at(eid)}}));

    auto updateManager = std::make_shared<UpdateManager>(
        event, handler, instanceIdDb, *descriptorMaps.back(), *componentInfoMaps.back(),
        false /* do not watch folder*/);

    aggregateUpdateManager->addUpdateManager(updateManager);

    inventoryItemManager.createInventoryItem(
        eid, firmwareDeviceNameMap.at(eid),
        utils::toString(activeCompImageSetVerStr), updateManager);
}

} // namespace fw_update

} // namespace pldm
