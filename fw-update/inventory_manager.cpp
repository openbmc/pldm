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
exec::task<int> InventoryManager::sendRecvPldmMsgOverMctp(
    mctp_eid_t eid, Request& request, const pldm_msg** responseMsg,
    size_t* responseLen)
{
    int rc = 0;
    try
    {
        std::tie(rc, *responseMsg, *responseLen) =
            co_await handler.sendRecvMsg(eid, std::move(request));
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("Send and Receive PLDM message over MCTP throw error - {ERROR}.",
              "ERROR", e);
        co_return PLDM_ERROR;
    }
    catch (const int& e)
    {
        error(
            "Send and Receive PLDM message over MCTP throw int error - {ERROR}.",
            "ERROR", e);
        co_return PLDM_ERROR;
    }

    co_return rc;
}

void InventoryManager::discoverFDs(const std::vector<mctp_eid_t>& eids)
{
    queuedMctpEids.emplace(eids);

    if (discoverFDsTaskHandle.has_value())
    {
        auto& [scope, rcOpt] = *discoverFDsTaskHandle;
        if (!rcOpt.has_value())
        {
            return;
        }
        stdexec::sync_wait(scope.on_empty());
        discoverFDsTaskHandle.reset();
    }
    auto& [scope, rcOpt] = discoverFDsTaskHandle.emplace();
    scope.spawn(discoverFDsTask() |
                    stdexec::then([&](int rc) { rcOpt.emplace(rc); }),
                exec::default_task_context<void>(exec::inline_scheduler{}));
}

exec::task<int> InventoryManager::discoverFDsTask()
{
    while (!queuedMctpEids.empty())
    {
        const auto& eids = queuedMctpEids.front();
        for (const auto& eid : eids)
        {
            co_await startFirmwareDiscoveryFlow(eid);
        }
        queuedMctpEids.pop();
    }

    co_return PLDM_SUCCESS;
}

exec::task<int> InventoryManager::startFirmwareDiscoveryFlow(mctp_eid_t eid)
{
    uint8_t rc = 0;

    rc = co_await queryDeviceIdentifiers(eid);

    if (rc != PLDM_SUCCESS)
    {
        descriptorMap.erase(eid);
        error(
            "Failed to execute the 'queryDeviceIdentifiers' function., EID={EID}, RC={RC} ",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    rc = co_await getFirmwareParameters(eid);

    if (rc != PLDM_SUCCESS)
    {
        descriptorMap.erase(eid);
        error("Failed to execute the 'getFirmwareParameters' function., "
              "EID={EID}, RC={RC} ",
              "EID", eid, "RC", rc);
    }

    co_return rc;
}

exec::task<int> InventoryManager::queryDeviceIdentifiers(mctp_eid_t eid)
{
    auto instanceId = instanceIdDb.next(eid);
    Request requestMsg(
        sizeof(pldm_msg_hdr) + PLDM_QUERY_DEVICE_IDENTIFIERS_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_query_device_identifiers_req(
        instanceId, PLDM_QUERY_DEVICE_IDENTIFIERS_REQ_BYTES, request);
    if (rc)
    {
        instanceIdDb.free(eid, instanceId);
        error("encode_query_device_identifiers_req failed, EID={EID}, RC={RC}",
              "EID", eid, "RC", rc);
        co_return rc;
    }

    const pldm_msg* responseMsg = NULL;
    size_t responseLen = 0;

    rc = co_await sendRecvPldmMsgOverMctp(eid, requestMsg, &responseMsg,
                                          &responseLen);

    if (rc)
    {
        error(
            "Failed to send QueryDeviceIdentifiers request, EID={EID}, RC={RC} ",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    rc = co_await parseQueryDeviceIdentifiersResponse(eid, responseMsg,
                                                      responseLen);
    if (rc)
    {
        error(
            "Failed to execute the 'parseQueryDeviceIdentifiersResponse' function., EID={EID}, RC={RC} ",
            "EID", eid, "RC", rc);

        co_return rc;
    }

    co_return rc;
}

exec::task<int> InventoryManager::parseQueryDeviceIdentifiersResponse(
    mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        error(
            "No response received for query device identifiers for endpoint ID {EID}",
            "EID", eid);
        co_return PLDM_ERROR;
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
        co_return PLDM_ERROR;
    }

    if (completionCode)
    {
        error(
            "Failed to query device identifiers response for endpoint ID {EID}, completion code {CC}",
            "EID", eid, "CC", completionCode);
        co_return PLDM_ERROR;
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
            co_return PLDM_ERROR;
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
                co_return PLDM_ERROR;
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

    co_return PLDM_SUCCESS;
}

exec::task<int> InventoryManager::queryDownstreamDevices(mctp_eid_t eid)
{
    Request requestMsg(sizeof(pldm_msg_hdr));
    auto instanceId = instanceIdDb.next(eid);
    auto request = new (requestMsg.data()) pldm_msg;
    auto rc = encode_query_downstream_devices_req(instanceId, request);

    if (rc)
    {
        instanceIdDb.free(eid, instanceId);
        error("encode_query_downstream_devices_req failed, EID={EID}, RC={RC}",
              "EID", eid, "RC", rc);
        co_return rc;
    }

    const pldm_msg* responseMsg = NULL;
    size_t responseLen = 0;

    rc = co_await sendRecvPldmMsgOverMctp(eid, requestMsg, &responseMsg,
                                          &responseLen);
    if (rc)
    {
        error(
            "Failed to send QueryDownstreamDevices request for endpoint ID {EID}, response code {RC}",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    rc = co_await parseQueryDownstreamDevicesResponse(eid, responseMsg,
                                                      responseLen);

    if (rc)
    {
        error("parseQueryDownstreamDeviceResponse failed, EID={EID}, RC={RC} ",
              "EID", eid, "RC", rc);
    }

    co_return rc;
}

exec::task<int> InventoryManager::parseQueryDownstreamDevicesResponse(
    mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen)
{
    if (!response || !respMsgLen)
    {
        error(
            "No response received for QueryDownstreamDevices for endpoint ID {EID}",
            "EID", eid);
        co_return PLDM_ERROR;
    }

    pldm_query_downstream_devices_resp downstreamDevicesResp{};
    auto rc = decode_query_downstream_devices_resp(response, respMsgLen,
                                                   &downstreamDevicesResp);
    if (rc)
    {
        error(
            "Decoding QueryDownstreamDevices response failed for endpoint ID {EID} with response code {RC}",
            "EID", eid, "RC", rc);
        co_return PLDM_ERROR;
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
            co_return PLDM_ERROR;
        default:
            error(
                "QueryDownstreamDevices response failed with error completion code for endpoint ID {EID} with completion code {CC}",
                "EID", eid, "CC", downstreamDevicesResp.completion_code);
            co_return PLDM_ERROR;
    }

    error("DownstreamDevicesResp.downstream_device_update_supported: {X}", "X",
          downstreamDevicesResp.downstream_device_update_supported);
    error("PLDM_FWUP_DOWNSTREAM_DEVICE_UPDATE_SUPPORTED: {X}", "X",
          PLDM_FWUP_DOWNSTREAM_DEVICE_UPDATE_SUPPORTED);
    error("PLDM_FWUP_DOWNSTREAM_DEVICE_UPDATE_NOT_SUPPORTED: {X}", "X",
          PLDM_FWUP_DOWNSTREAM_DEVICE_UPDATE_NOT_SUPPORTED);
    switch (downstreamDevicesResp.downstream_device_update_supported)
    {
        case PLDM_FWUP_DOWNSTREAM_DEVICE_UPDATE_SUPPORTED:
        {
            /** DataTransferHandle will be skipped when TransferOperationFlag is
             *  `GetFirstPart`. Use 0x0 as default by following example in
             *  Figure 9 in DSP0267 1.1.0
             */
            auto rc = co_await queryDownstreamIdentifiers(eid, 0x0,
                                                          PLDM_GET_FIRSTPART);
            if (rc)
            {
                error(
                    "Failed to send QueryDownstreamIdentifiers request for endpoint ID {EID}",
                    "EID", eid);
            }
            break;
        }
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
            co_return PLDM_ERROR;
    }
    co_return PLDM_SUCCESS;
}

exec::task<int> InventoryManager::queryDownstreamIdentifiers(
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
        co_return rc;
    }

    const pldm_msg* responseMsg = NULL;
    size_t responseLen = 0;

    rc = co_await sendRecvPldmMsgOverMctp(eid, requestMsg, &responseMsg,
                                          &responseLen);
    if (rc)
    {
        error(
            "Failed to send QueryDownstreamIdentifiers request for endpoint ID {EID} with response code {RC}",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    rc = co_await parseQueryDownstreamIdentifiersResponse(
        eid, responseMsg, responseLen);

    if (rc)
    {
        error(
            "parseQueryDownstreamIdentifiersResponse failed, EID={EID}, RC={RC} ",
            "EID", eid, "RC", rc);
    }
    co_return rc;
}

exec::task<int> InventoryManager::parseQueryDownstreamIdentifiersResponse(
    mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen)
{
    if (!response || !respMsgLen)
    {
        error(
            "No response received for QueryDownstreamIdentifiers for endpoint ID {EID}",
            "EID", eid);
        descriptorMap.erase(eid);
        co_return PLDM_ERROR;
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
        co_return PLDM_ERROR;
    }

    if (downstreamIds.completion_code)
    {
        error(
            "QueryDownstreamIdentifiers response failed with error completion code for endpoint ID {EID} with completion code {CC}",
            "EID", eid, "CC", unsigned(downstreamIds.completion_code));
        co_return PLDM_ERROR;
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
                    co_return PLDM_ERROR;
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
            co_return PLDM_ERROR;
        }
        downstreamDevices->emplace(dev.downstream_device_index, descriptors);
    }
    if (rc)
    {
        error(
            "Failed to decode downstream devices from iterator for endpoint ID {EID} with response code {RC}",
            "EID", eid, "RC", rc);
        co_return PLDM_ERROR;
    }

    switch (downstreamIds.transfer_flag)
    {
        case PLDM_START:
            downstreamDescriptorMap.insert_or_assign(
                eid, std::move(initialDownstreamDevices));
            [[fallthrough]];
        case PLDM_MIDDLE:
        {
            auto rc = co_await queryDownstreamIdentifiers(
                eid, downstreamIds.next_data_transfer_handle,
                PLDM_GET_NEXTPART);
            if (rc)
            {
                error(
                    "Failed to send QueryDownstreamIdentifiers request for endpoint ID {EID}",
                    "EID", eid);
            }
            break;
        }
        case PLDM_START_AND_END:
            downstreamDescriptorMap.insert_or_assign(
                eid, std::move(initialDownstreamDevices));
            /** DataTransferHandle will be skipped when TransferOperationFlag is
             *  `GetFirstPart`. Use 0x0 as default by following example in
             *  Figure 9 in DSP0267 1.1.0
             */
            [[fallthrough]];
        case PLDM_END:
        {
            auto rc = co_await getDownstreamFirmwareParameters(
                eid, 0x0, PLDM_GET_FIRSTPART);
            if (rc)
            {
                error(
                    "Failed to send GetDownstreamFirmwareParameters request for endpoint ID {EID}",
                    "EID", eid);
            }

            break;
        }
    }
    co_return PLDM_SUCCESS;
}

exec::task<int> InventoryManager::getDownstreamFirmwareParameters(
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
        co_return rc;
    }

    const pldm_msg* responseMsg = NULL;
    size_t responseLen = 0;

    rc = co_await sendRecvPldmMsgOverMctp(eid, requestMsg, &responseMsg,
                                          &responseLen);
    if (rc)
    {
        error(
            "Failed to send QueryDownstreamFirmwareParameters request for endpoint ID {EID} with response code {RC}",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    rc = co_await parseGetDownstreamFirmwareParametersResponse(
        eid, responseMsg, responseLen);

    if (rc)
    {
        error(
            "parseGetDownstreamFirmwareParametersResponse failed, EID={EID}, RC={RC} ",
            "EID", eid, "RC", rc);
    }
    co_return rc;
}

exec::task<int> InventoryManager::parseGetDownstreamFirmwareParametersResponse(
    mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen)
{
    if (!response || !respMsgLen)
    {
        error(
            "No response received for QueryDownstreamFirmwareParameters for endpoint ID {EID}",
            "EID", eid);
        descriptorMap.erase(eid);
        co_return PLDM_ERROR;
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
        co_return PLDM_ERROR;
    }

    if (resp.completion_code)
    {
        error(
            "QueryDownstreamFirmwareParameters response failed with error completion code for endpoint ID {EID} with completion code {CC}",
            "EID", eid, "CC", resp.completion_code);
        co_return PLDM_ERROR;
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
        co_return PLDM_ERROR;
    }

    switch (resp.transfer_flag)
    {
        case PLDM_START:
        case PLDM_MIDDLE:
        {
            auto rc = co_await getDownstreamFirmwareParameters(
                eid, resp.next_data_transfer_handle, PLDM_GET_NEXTPART);
            if (rc)
            {
                error(
                    "Failed to send GetDownstreamFirmwareParameters request for endpoint ID {EID}",
                    "EID", eid);
            }
            break;
        }
    }
    co_return PLDM_SUCCESS;
}

exec::task<int> InventoryManager::getFirmwareParameters(mctp_eid_t eid)
{
    auto instanceId = instanceIdDb.next(eid);
    Request requestMsg(
        sizeof(pldm_msg_hdr) + PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_get_firmware_parameters_req(
        instanceId, PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES, request);
    if (rc)
    {
        instanceIdDb.free(eid, instanceId);
        error(
            "Failed to encode get firmware parameters req for endpoint ID {EID}, response code {RC}",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    const pldm_msg* responseMsg = NULL;
    size_t responseLen = 0;

    rc = co_await sendRecvPldmMsgOverMctp(eid, requestMsg, &responseMsg,
                                          &responseLen);

    if (rc)
    {
        error(
            "Failed to send get firmware parameters request for endpoint ID {EID}, response code {RC}",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    rc = co_await parseGetFWParametersResponse(eid, responseMsg, responseLen);

    if (rc)
    {
        error("parseGetFWParametersResponse failed, EID={EID}, RC={RC} ", "EID",
              eid, "RC", rc);
    }

    co_return rc;
}

exec::task<int> InventoryManager::parseGetFWParametersResponse(
    mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        error(
            "No response received for get firmware parameters for endpoint ID {EID}",
            "EID", eid);
        descriptorMap.erase(eid);
        co_return PLDM_ERROR;
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
        co_return rc;
    }

    if (fwParams.completion_code)
    {
        auto fw_param_cc = fwParams.completion_code;
        error(
            "Failed to get firmware parameters response for endpoint ID {EID}, completion code {CC}",
            "EID", eid, "CC", fw_param_cc);
        co_return PLDM_ERROR;
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
            co_return PLDM_ERROR;
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
    co_return PLDM_SUCCESS;
}

} // namespace fw_update

} // namespace pldm
