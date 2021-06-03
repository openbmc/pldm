#include "libpldm/base.h"
#include "libpldm/firmware_update.h"

#include <gtest/gtest.h>

constexpr auto hdrSize = sizeof(pldm_msg_hdr);

TEST(QueryDeviceIdentifiers, goodPathEncodeRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr)> requestMsg{};
    auto requestPtr = reinterpret_cast<pldm_msg*>(requestMsg.data());

    uint8_t instanceId = 0x01;

    auto rc = encode_query_device_identifiers_req(
        instanceId, PLDM_QUERY_DEVICE_IDENTIFIERS_REQ_BYTES, requestPtr);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(requestPtr->hdr.request, PLDM_REQUEST);
    EXPECT_EQ(requestPtr->hdr.instance_id, instanceId);
    EXPECT_EQ(requestPtr->hdr.type, PLDM_FWUP);
    EXPECT_EQ(requestPtr->hdr.command, PLDM_QUERY_DEVICE_IDENTIFIERS);
}

TEST(QueryDeviceIdentifiers, goodPathDecodeResponse)
{
    // descriptorDataLen is not fixed here taking it as 6
    constexpr uint8_t descriptorDataLen = 6;
    std::array<uint8_t, hdrSize +
                            sizeof(struct pldm_query_device_identifiers_resp) +
                            descriptorDataLen>
        responseMsg{};
    auto inResp = reinterpret_cast<struct pldm_query_device_identifiers_resp*>(
        responseMsg.data() + hdrSize);

    inResp->completion_code = PLDM_SUCCESS;
    inResp->device_identifiers_len = htole32(descriptorDataLen);
    inResp->descriptor_count = 1;

    // filling descriptor data
    std::fill_n(responseMsg.data() + hdrSize +
                    sizeof(struct pldm_query_device_identifiers_resp),
                descriptorDataLen, 0xFF);

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());
    uint8_t completionCode = PLDM_SUCCESS;
    uint32_t deviceIdentifiersLen = 0;
    uint8_t descriptorCount = 0;
    uint8_t* outDescriptorData = nullptr;

    auto rc = decode_query_device_identifiers_resp(
        response, responseMsg.size() - hdrSize, &completionCode,
        &deviceIdentifiersLen, &descriptorCount, &outDescriptorData);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(completionCode, PLDM_SUCCESS);
    EXPECT_EQ(deviceIdentifiersLen, inResp->device_identifiers_len);
    EXPECT_EQ(descriptorCount, inResp->descriptor_count);
    EXPECT_EQ(true,
              std::equal(outDescriptorData,
                         outDescriptorData + deviceIdentifiersLen,
                         responseMsg.begin() + hdrSize +
                             sizeof(struct pldm_query_device_identifiers_resp),
                         responseMsg.end()));
}

TEST(GetFirmwareParameters, goodPathEncodeRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr)> requestMsg{};
    auto requestPtr = reinterpret_cast<pldm_msg*>(requestMsg.data());
    uint8_t instanceId = 0x01;

    auto rc = encode_get_firmware_parameters_req(
        instanceId, PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES, requestPtr);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(requestPtr->hdr.request, PLDM_REQUEST);
    EXPECT_EQ(requestPtr->hdr.instance_id, instanceId);
    EXPECT_EQ(requestPtr->hdr.type, PLDM_FWUP);
    EXPECT_EQ(requestPtr->hdr.command, PLDM_GET_FIRMWARE_PARAMETERS);
}

TEST(GetFirmwareParameters, goodPathDecodeResponseComponentSetInfo)
{
    // Random value for capabilities during update
    constexpr uint32_t capabilitiesDuringUpdate = 0xBADBEEFE;
    // Random value for component count
    constexpr uint16_t componentCount = 0xAABB;
    // ActiveCompImageSetVerStrLen is not fixed here taking it as 8
    constexpr uint8_t activeCompImageSetVerStrLen = 8;
    // PendingCompImageSetVerStrLen is not fixed here taking it as 8
    constexpr uint8_t pendingCompImageSetVerStrLen = 8;
    constexpr size_t payloadLen =
        sizeof(struct pldm_get_firmware_parameters_resp) +
        activeCompImageSetVerStrLen + pendingCompImageSetVerStrLen;

    std::array<uint8_t, hdrSize + payloadLen> response{};
    auto inResp = reinterpret_cast<struct pldm_get_firmware_parameters_resp*>(
        response.data() + hdrSize);
    inResp->completion_code = PLDM_SUCCESS;
    inResp->capabilities_during_update.value =
        htole32(capabilitiesDuringUpdate);
    inResp->comp_count = htole16(componentCount);
    inResp->active_comp_image_set_ver_str_type = 1;
    inResp->active_comp_image_set_ver_str_len = activeCompImageSetVerStrLen;
    inResp->pending_comp_image_set_ver_str_type = 1;
    inResp->pending_comp_image_set_ver_str_len = pendingCompImageSetVerStrLen;

    constexpr size_t activeCompImageSetVerStrPos =
        hdrSize + sizeof(struct pldm_get_firmware_parameters_resp);
    // filling default values for ActiveComponentImageSetVersionString
    std::fill_n(response.data() + activeCompImageSetVerStrPos,
                activeCompImageSetVerStrLen, 0xFF);
    constexpr size_t pendingCompImageSetVerStrPos =
        hdrSize + sizeof(struct pldm_get_firmware_parameters_resp) +
        activeCompImageSetVerStrLen;
    // filling default values for ActiveComponentImageSetVersionString
    std::fill_n(response.data() + pendingCompImageSetVerStrPos,
                pendingCompImageSetVerStrLen, 0xFF);

    auto responseMsg = reinterpret_cast<pldm_msg*>(response.data());
    struct pldm_get_firmware_parameters_resp outResp;
    struct variable_field outActiveCompImageSetVerStr;
    struct variable_field outPendingCompImageSetVerStr;

    auto rc = decode_get_firmware_parameters_resp_comp_set_info(
        responseMsg, payloadLen, &outResp, &outActiveCompImageSetVerStr,
        &outPendingCompImageSetVerStr);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(outResp.completion_code, PLDM_SUCCESS);
    EXPECT_EQ(outResp.capabilities_during_update.value,
              capabilitiesDuringUpdate);
    EXPECT_EQ(outResp.comp_count, componentCount);
    EXPECT_EQ(inResp->active_comp_image_set_ver_str_type,
              outResp.active_comp_image_set_ver_str_type);
    EXPECT_EQ(inResp->active_comp_image_set_ver_str_len,
              outResp.active_comp_image_set_ver_str_len);
    EXPECT_EQ(0, memcmp(outActiveCompImageSetVerStr.ptr,
                        response.data() + activeCompImageSetVerStrPos,
                        outActiveCompImageSetVerStr.length));

    EXPECT_EQ(inResp->pending_comp_image_set_ver_str_type,
              outResp.pending_comp_image_set_ver_str_type);
    EXPECT_EQ(inResp->pending_comp_image_set_ver_str_len,
              outResp.pending_comp_image_set_ver_str_len);
    EXPECT_EQ(0, memcmp(outPendingCompImageSetVerStr.ptr,
                        response.data() + pendingCompImageSetVerStrPos,
                        outPendingCompImageSetVerStr.length));
}

TEST(GetFirmwareParameters, goodPathDecodeComponentParameterEntry)
{
    // Random value for component classification
    constexpr uint16_t compClassification = 0x0A0B;
    // Random value for component classification
    constexpr uint16_t compIdentifier = 0x0C0D;
    // Random value for component classification
    constexpr uint32_t timestamp = 0X12345678;
    // Random value for component activation methods
    constexpr uint16_t compActivationMethods = 0xBBDD;
    // Random value for capabilities during update
    constexpr uint32_t capabilitiesDuringUpdate = 0xBADBEEFE;

    // ActiveCompImageSetVerStrLen is not fixed here taking it as 8
    constexpr uint8_t activeCompVerStrLen = 8;
    // PendingCompImageSetVerStrLen is not fixed here taking it as 8
    constexpr uint8_t pendingCompVerStrLen = 8;
    constexpr size_t entryLength =
        sizeof(struct pldm_component_parameter_entry) + activeCompVerStrLen +
        pendingCompVerStrLen;
    std::array<uint8_t, entryLength> entry{};

    auto inEntry =
        reinterpret_cast<struct pldm_component_parameter_entry*>(entry.data());

    inEntry->comp_classification = htole16(compClassification);
    inEntry->comp_identifier = htole16(compIdentifier);
    inEntry->comp_classification_index = 0x0F;
    inEntry->active_comp_comparison_stamp = htole32(timestamp);
    inEntry->active_comp_ver_str_type = 1;
    inEntry->active_comp_ver_str_len = activeCompVerStrLen;
    std::fill_n(inEntry->active_comp_release_date,
                sizeof(inEntry->active_comp_release_date), 0xFF);
    inEntry->pending_comp_comparison_stamp = htole32(timestamp);
    inEntry->pending_comp_ver_str_type = 1;
    inEntry->pending_comp_ver_str_len = pendingCompVerStrLen;
    std::fill_n(inEntry->pending_comp_release_date,
                sizeof(inEntry->pending_comp_release_date), 0xFF);
    inEntry->comp_activation_methods.value = htole16(compActivationMethods);
    inEntry->capabilities_during_update.value =
        htole32(capabilitiesDuringUpdate);
    constexpr auto activeCompVerStrPos =
        sizeof(struct pldm_component_parameter_entry);
    std::fill_n(entry.data() + activeCompVerStrPos, activeCompVerStrLen, 0xAA);
    constexpr auto pendingCompVerStrPos =
        activeCompVerStrPos + activeCompVerStrLen;
    std::fill_n(entry.data() + pendingCompVerStrPos, pendingCompVerStrLen,
                0xBB);

    struct pldm_component_parameter_entry outEntry;
    struct variable_field outActiveCompVerStr;
    struct variable_field outPendingCompVerStr;

    auto rc = decode_get_firmware_parameters_resp_comp_entry(
        entry.data(), entryLength, &outEntry, &outActiveCompVerStr,
        &outPendingCompVerStr);

    EXPECT_EQ(rc, PLDM_SUCCESS);

    EXPECT_EQ(outEntry.comp_classification, compClassification);
    EXPECT_EQ(outEntry.comp_identifier, compIdentifier);
    EXPECT_EQ(inEntry->comp_classification_index,
              outEntry.comp_classification_index);
    EXPECT_EQ(outEntry.active_comp_comparison_stamp, timestamp);
    EXPECT_EQ(inEntry->active_comp_ver_str_type,
              outEntry.active_comp_ver_str_type);
    EXPECT_EQ(inEntry->active_comp_ver_str_len,
              outEntry.active_comp_ver_str_len);
    EXPECT_EQ(0, memcmp(inEntry->active_comp_release_date,
                        outEntry.active_comp_release_date,
                        sizeof(inEntry->active_comp_release_date)));
    EXPECT_EQ(outEntry.pending_comp_comparison_stamp, timestamp);
    EXPECT_EQ(inEntry->pending_comp_ver_str_type,
              outEntry.pending_comp_ver_str_type);
    EXPECT_EQ(inEntry->pending_comp_ver_str_len,
              outEntry.pending_comp_ver_str_len);
    EXPECT_EQ(0, memcmp(inEntry->pending_comp_release_date,
                        outEntry.pending_comp_release_date,
                        sizeof(inEntry->pending_comp_release_date)));
    EXPECT_EQ(outEntry.comp_activation_methods.value, compActivationMethods);
    EXPECT_EQ(outEntry.capabilities_during_update.value,
              capabilitiesDuringUpdate);

    EXPECT_EQ(0, memcmp(outActiveCompVerStr.ptr,
                        entry.data() + activeCompVerStrPos,
                        outActiveCompVerStr.length));
    EXPECT_EQ(0, memcmp(outPendingCompVerStr.ptr,
                        entry.data() + pendingCompVerStrPos,
                        outPendingCompVerStr.length));
}

TEST(RequestUpdate, testGoodEncodeRequest)
{
    uint8_t instanceId = 0x01;
    // Component Image Set Version String Length is not fixed here taking it as
    // 6
    constexpr uint8_t compImgSetVerStrLen = 6;

    std::array<uint8_t, compImgSetVerStrLen> compImgSetVerStrArr;
    struct variable_field inCompImgSetVerStr;
    inCompImgSetVerStr.ptr = compImgSetVerStrArr.data();
    inCompImgSetVerStr.length = compImgSetVerStrLen;

    struct pldm_request_update_req inReq = {};

    inReq.max_transfer_size = 32;
    inReq.no_of_comp = 1;
    inReq.max_outstand_transfer_req = 1;
    inReq.pkg_data_len = 0;
    inReq.comp_image_set_ver_str_type = PLDM_COMP_VER_STR_TYPE_UNKNOWN;
    inReq.comp_image_set_ver_str_len = compImgSetVerStrLen;

    std::fill(compImgSetVerStrArr.data(), compImgSetVerStrArr.end(), 0xFF);

    std::array<uint8_t, hdrSize + sizeof(struct pldm_request_update_req) +
                            compImgSetVerStrLen>
        outReq;

    auto msg = reinterpret_cast<pldm_msg*>(outReq.data());

    auto rc = encode_request_update_req(instanceId, msg,
                                        sizeof(struct pldm_request_update_req) +
                                            inCompImgSetVerStr.length,
                                        &inReq, &inCompImgSetVerStr);

    auto request = (struct pldm_request_update_req*)(outReq.data() + hdrSize);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(msg->hdr.request, PLDM_REQUEST);
    EXPECT_EQ(msg->hdr.instance_id, instanceId);
    EXPECT_EQ(msg->hdr.type, PLDM_FWUP);
    EXPECT_EQ(msg->hdr.command, PLDM_REQUEST_UPDATE);
    EXPECT_EQ(le32toh(request->max_transfer_size), inReq.max_transfer_size);
    EXPECT_EQ(le16toh(request->no_of_comp), inReq.no_of_comp);
    EXPECT_EQ(request->max_outstand_transfer_req,
              inReq.max_outstand_transfer_req);
    EXPECT_EQ(le16toh(request->pkg_data_len), inReq.pkg_data_len);
    EXPECT_EQ(request->comp_image_set_ver_str_type,
              inReq.comp_image_set_ver_str_type);
    EXPECT_EQ(request->comp_image_set_ver_str_len,
              inReq.comp_image_set_ver_str_len);
    EXPECT_EQ(true,
              std::equal(compImgSetVerStrArr.begin(), compImgSetVerStrArr.end(),
                         outReq.data() + hdrSize +
                             sizeof(struct pldm_request_update_req)));
}

TEST(RequestUpdate, testBadEncodeRequest)
{
    uint8_t instanceId = 0x01;
    constexpr uint8_t compImgSetVerStrLen = 6;

    std::array<uint8_t, compImgSetVerStrLen> compImgSetVerStrArr;
    struct variable_field inCompImgSetVerStr;
    inCompImgSetVerStr.ptr = compImgSetVerStrArr.data();
    inCompImgSetVerStr.length = compImgSetVerStrLen;

    struct pldm_request_update_req inReq = {};

    std::fill(compImgSetVerStrArr.data(), compImgSetVerStrArr.end(), 0xFF);

    std::array<uint8_t, hdrSize + sizeof(struct pldm_request_update_req) +
                            compImgSetVerStrLen>
        outReq;

    auto msg = reinterpret_cast<pldm_msg*>(outReq.data());

    auto rc = encode_request_update_req(instanceId, 0,
                                        sizeof(struct pldm_request_update_req) +
                                            inCompImgSetVerStr.length,
                                        &inReq, &inCompImgSetVerStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = encode_request_update_req(instanceId, msg, 0, &inReq,
                                   &inCompImgSetVerStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    rc = encode_request_update_req(instanceId, msg,
                                   sizeof(struct pldm_request_update_req) +
                                       inCompImgSetVerStr.length,
                                   NULL, &inCompImgSetVerStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = encode_request_update_req(instanceId, msg,
                                   sizeof(struct pldm_request_update_req) +
                                       inCompImgSetVerStr.length,
                                   &inReq, NULL);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inReq.max_transfer_size = 30;
    inReq.no_of_comp = 0;
    inReq.max_outstand_transfer_req = 0;
    inReq.pkg_data_len = 0;
    inReq.comp_image_set_ver_str_type = 10;
    inReq.comp_image_set_ver_str_len = 0;

    rc = encode_request_update_req(instanceId, msg,
                                   sizeof(struct pldm_request_update_req) +
                                       inCompImgSetVerStr.length,
                                   &inReq, &inCompImgSetVerStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inCompImgSetVerStr.ptr = NULL;
    inCompImgSetVerStr.length = 0;

    rc = encode_request_update_req(instanceId, msg,
                                   sizeof(struct pldm_request_update_req) +
                                       inCompImgSetVerStr.length,
                                   &inReq, &inCompImgSetVerStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}
TEST(RequestUpdate, testGoodDecodeResponse)
{
    uint8_t completionCode = PLDM_ERROR;
    uint16_t fdMetaDataLen = 0;
    uint8_t fdPkgData = 0;

    std::array<uint8_t, hdrSize + sizeof(struct pldm_request_update_resp)>
        responseMsg{};
    struct pldm_request_update_resp* inResp =
        reinterpret_cast<struct pldm_request_update_resp*>(responseMsg.data() +
                                                           hdrSize);
    inResp->completion_code = PLDM_SUCCESS;
    inResp->fd_meta_data_len = 0x0F;
    inResp->fd_pkg_data = 0x0F;

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc =
        decode_request_update_resp(response, responseMsg.size() - hdrSize,
                                   &completionCode, &fdMetaDataLen, &fdPkgData);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(completionCode, PLDM_SUCCESS);
    EXPECT_EQ(fdMetaDataLen, htole16(inResp->fd_meta_data_len));
    EXPECT_EQ(fdPkgData, inResp->fd_pkg_data);
}

TEST(RequestUpdate, testBadDecodeResponse)
{
    uint8_t completionCode = PLDM_ERROR;
    uint16_t fdMetaDataLen = 0;
    uint8_t fdPkgData = 0;

    std::array<uint8_t, hdrSize + sizeof(struct pldm_request_update_resp)>
        responseMsg{};
    struct pldm_request_update_resp* inResp =
        reinterpret_cast<struct pldm_request_update_resp*>(responseMsg.data() +
                                                           hdrSize);
    inResp->completion_code = PLDM_SUCCESS;
    inResp->fd_meta_data_len = 0x0F;
    inResp->fd_pkg_data = 0x0F;

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc =
        decode_request_update_resp(NULL, responseMsg.size() - hdrSize,
                                   &completionCode, &fdMetaDataLen, &fdPkgData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_request_update_resp(response, 0, &completionCode,
                                    &fdMetaDataLen, &fdPkgData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    rc = decode_request_update_resp(response, responseMsg.size() - hdrSize,
                                    NULL, &fdMetaDataLen, &fdPkgData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_request_update_resp(response, responseMsg.size() - hdrSize,
                                    &completionCode, NULL, &fdPkgData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_request_update_resp(response, responseMsg.size() - hdrSize,
                                    &completionCode, &fdMetaDataLen, NULL);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}
TEST(PassComponentTable, testGoodEncodeRequest)
{
    uint8_t instanceId = 0x01;
    // Component Version String Length is not fixed here taking it as 6
    constexpr uint8_t compVerStrLen = 6;

    std::array<uint8_t, compVerStrLen> compVerStrArr;
    struct variable_field inCompVerStr;
    inCompVerStr.ptr = compVerStrArr.data();
    inCompVerStr.length = compVerStrLen;

    struct pldm_pass_component_table_req inReq = {};

    inReq.transfer_flag = PLDM_START;
    inReq.comp_classification = COMP_UNKNOWN;
    inReq.comp_identifier = 0x00;
    inReq.comp_classification_index = 0x00;
    inReq.comp_comparison_stamp = 0;
    inReq.comp_ver_str_type = PLDM_COMP_VER_STR_TYPE_UNKNOWN;
    inReq.comp_ver_str_len = compVerStrLen;

    std::fill(compVerStrArr.data(), compVerStrArr.end(), 0xFF);

    std::array<uint8_t, hdrSize + sizeof(struct pldm_pass_component_table_req) +
                            compVerStrLen>
        outReq;

    auto msg = (struct pldm_msg*)outReq.data();

    auto rc = encode_pass_component_table_req(
        instanceId, msg,
        sizeof(struct pldm_pass_component_table_req) + inCompVerStr.length,
        &inReq, &inCompVerStr);

    auto request =
        (struct pldm_pass_component_table_req*)(outReq.data() + hdrSize);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(msg->hdr.request, PLDM_REQUEST);
    EXPECT_EQ(msg->hdr.instance_id, instanceId);
    EXPECT_EQ(msg->hdr.type, PLDM_FWUP);
    EXPECT_EQ(msg->hdr.command, PLDM_PASS_COMPONENT_TABLE);
    EXPECT_EQ(request->transfer_flag, inReq.transfer_flag);
    EXPECT_EQ(le16toh(request->comp_classification), inReq.comp_classification);
    EXPECT_EQ(le16toh(request->comp_identifier), inReq.comp_identifier);
    EXPECT_EQ(request->comp_classification_index,
              inReq.comp_classification_index);
    EXPECT_EQ(le32toh(request->comp_comparison_stamp),
              inReq.comp_comparison_stamp);
    EXPECT_EQ(request->comp_ver_str_type, inReq.comp_ver_str_type);
    EXPECT_EQ(request->comp_ver_str_len, inReq.comp_ver_str_len);
    EXPECT_EQ(true,
              std::equal(compVerStrArr.begin(), compVerStrArr.end(),
                         outReq.data() + hdrSize +
                             sizeof(struct pldm_pass_component_table_req)));
}

TEST(PassComponentTable, testBadEncodeRequest)
{
    uint8_t instanceId = 0x01;
    constexpr uint8_t compVerStrLen = 6;

    std::array<uint8_t, compVerStrLen> compVerStrArr;
    struct variable_field inCompVerStr;
    inCompVerStr.ptr = compVerStrArr.data();
    inCompVerStr.length = compVerStrLen;

    struct pldm_pass_component_table_req inReq = {};

    std::fill(compVerStrArr.data(), compVerStrArr.end(), 0xFF);

    std::array<uint8_t, hdrSize + sizeof(struct pldm_pass_component_table_req) +
                            compVerStrLen>
        outReq;

    auto msg = (struct pldm_msg*)outReq.data();

    auto rc = encode_pass_component_table_req(
        instanceId, 0,
        sizeof(struct pldm_pass_component_table_req) + inCompVerStr.length,
        &inReq, &inCompVerStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = encode_pass_component_table_req(instanceId, msg, 0, &inReq,
                                         &inCompVerStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    rc = encode_pass_component_table_req(
        instanceId, msg,
        sizeof(struct pldm_pass_component_table_req) + inCompVerStr.length,
        NULL, &inCompVerStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = encode_pass_component_table_req(
        instanceId, msg,
        sizeof(struct pldm_pass_component_table_req) + inCompVerStr.length,
        &inReq, NULL);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inReq.transfer_flag = PLDM_START;
    inCompVerStr.ptr = NULL;
    inCompVerStr.length = 0;

    rc = encode_pass_component_table_req(
        instanceId, msg,
        sizeof(struct pldm_pass_component_table_req) + inCompVerStr.length,
        &inReq, &inCompVerStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inReq.transfer_flag = PLDM_START_AND_END;
    inReq.comp_classification = COMP_SOFTWARE_BUNDLE + 1;
    inReq.comp_identifier = 0x00;
    inReq.comp_classification_index = 0x00;
    inReq.comp_comparison_stamp = 0;
    inReq.comp_ver_str_type = PLDM_COMP_UTF_16BE + 1;
    inReq.comp_ver_str_len = 0;

    rc = encode_pass_component_table_req(
        instanceId, msg,
        sizeof(struct pldm_pass_component_table_req) + inCompVerStr.length,
        &inReq, &inCompVerStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inReq.transfer_flag = 0x4;
    inReq.comp_classification = 0x000E;
    inReq.comp_identifier = 0x00;
    inReq.comp_classification_index = 0x00;
    inReq.comp_comparison_stamp = 0;
    inReq.comp_ver_str_type = 6;
    inReq.comp_ver_str_len = 0;

    rc = encode_pass_component_table_req(
        instanceId, msg,
        sizeof(struct pldm_pass_component_table_req) + inCompVerStr.length,
        &inReq, &inCompVerStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inReq.transfer_flag = PLDM_START;
    inReq.comp_classification = COMP_UNKNOWN - 1;
    inReq.comp_identifier = 0x00;
    inReq.comp_classification_index = 0x00;
    inReq.comp_comparison_stamp = 0;
    inReq.comp_ver_str_type = PLDM_COMP_VER_STR_TYPE_UNKNOWN - 1;
    inReq.comp_ver_str_len = 0;

    rc = encode_pass_component_table_req(
        instanceId, msg,
        sizeof(struct pldm_pass_component_table_req) + inCompVerStr.length,
        &inReq, &inCompVerStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inReq.comp_ver_str_len = 6;
    inReq.comp_classification = COMP_UNKNOWN;
    inReq.comp_ver_str_type = PLDM_COMP_VER_STR_TYPE_UNKNOWN;
    inCompVerStr.ptr = compVerStrArr.data();
    inCompVerStr.length = 6;
    inReq.transfer_flag = PLDM_START - 1;
    rc = encode_pass_component_table_req(
        instanceId, msg,
        sizeof(struct pldm_pass_component_table_req) + inCompVerStr.length,
        &inReq, &inCompVerStr);
    EXPECT_EQ(rc, PLDM_INVALID_TRANSFER_OPERATION_FLAG);

    inReq.transfer_flag = PLDM_START_AND_END + 1;
    rc = encode_pass_component_table_req(
        instanceId, msg,
        sizeof(struct pldm_pass_component_table_req) + inCompVerStr.length,
        &inReq, &inCompVerStr);
    EXPECT_EQ(rc, PLDM_INVALID_TRANSFER_OPERATION_FLAG);
}

TEST(PassComponentTable, testGoodDecodeResponse)
{
    uint8_t completionCode = PLDM_ERROR;
    uint8_t compResp = PLDM_COMP_CAN_BE_UPDATEABLE;
    uint8_t compRespCode = COMP_COMPARISON_STAMP_IDENTICAL;

    std::array<uint8_t, hdrSize + sizeof(struct pldm_pass_component_table_resp)>
        responseMsg{};
    struct pldm_pass_component_table_resp* inResp =
        reinterpret_cast<struct pldm_pass_component_table_resp*>(
            responseMsg.data() + hdrSize);
    inResp->completion_code = PLDM_SUCCESS;
    inResp->comp_resp = 0;
    inResp->comp_resp_code = 1;

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = decode_pass_component_table_resp(
        response, responseMsg.size() - hdrSize, &completionCode, &compResp,
        &compRespCode);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(completionCode, PLDM_SUCCESS);
    EXPECT_EQ(compResp, inResp->comp_resp);
    EXPECT_EQ(compRespCode, inResp->comp_resp_code);
}

TEST(PassComponentTable, testBadDecodeResponse)
{
    uint8_t completionCode = PLDM_ERROR;
    uint8_t compResp = PLDM_COMP_CAN_BE_UPDATEABLE;
    uint8_t compRespCode = INVALID_COMP_COMPARISON_STAMP;

    std::array<uint8_t, hdrSize + sizeof(struct pldm_pass_component_table_resp)>
        responseMsg{};
    struct pldm_pass_component_table_resp* inResp =
        reinterpret_cast<struct pldm_pass_component_table_resp*>(
            responseMsg.data() + hdrSize);
    inResp->completion_code = PLDM_SUCCESS;
    inResp->comp_resp = 1;
    inResp->comp_resp_code = 3;

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = decode_pass_component_table_resp(
        NULL, responseMsg.size() - hdrSize, &completionCode, &compResp,
        &compRespCode);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_pass_component_table_resp(response, 0, &completionCode,
                                          &compResp, &compRespCode);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    rc = decode_pass_component_table_resp(
        response, responseMsg.size() - hdrSize, NULL, &compResp, &compRespCode);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc =
        decode_pass_component_table_resp(response, responseMsg.size() - hdrSize,
                                         &completionCode, NULL, &compRespCode);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc =
        decode_pass_component_table_resp(response, responseMsg.size() - hdrSize,
                                         &completionCode, &compResp, NULL);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->comp_resp = PLDM_COMP_CAN_BE_UPDATEABLE - 1;
    rc = decode_pass_component_table_resp(
        response, responseMsg.size() - hdrSize, &completionCode, &compResp,
        &compRespCode);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->comp_resp = PLDM_COMP_MAY_BE_UPDATEABLE + 1;
    rc = decode_pass_component_table_resp(
        response, responseMsg.size() - hdrSize, &completionCode, &compResp,
        &compRespCode);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->comp_resp = 0x09;
    rc = decode_pass_component_table_resp(
        response, responseMsg.size() - hdrSize, &completionCode, &compResp,
        &compRespCode);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->comp_resp_code = FD_VENDOR_COMP_STATUS_CODE_RANGE_MIN - 1;

    rc = decode_pass_component_table_resp(
        response, responseMsg.size() - hdrSize, &completionCode, &compResp,
        &compRespCode);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->comp_resp_code = FD_VENDOR_COMP_STATUS_CODE_RANGE_MAX + 1;

    rc = decode_pass_component_table_resp(
        response, responseMsg.size() - hdrSize, &completionCode, &compResp,
        &compRespCode);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->comp_resp_code = COMP_CAN_BE_UPDATED - 1;

    rc = decode_pass_component_table_resp(
        response, responseMsg.size() - hdrSize, &completionCode, &compResp,
        &compRespCode);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->comp_resp_code = 0xFF;

    rc = decode_pass_component_table_resp(
        response, responseMsg.size() - hdrSize, &completionCode, &compResp,
        &compRespCode);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(UpdateComponent, testGoodEncodeRequest)
{
    uint8_t instanceId = 0x01;
    // Component Version String Length is not fixed here taking it as 6
    constexpr uint8_t compVerStrLen = 6;

    std::array<uint8_t, compVerStrLen> compVerStrArr;
    struct variable_field inCompVerStr;
    inCompVerStr.ptr = compVerStrArr.data();
    inCompVerStr.length = compVerStrLen;

    struct pldm_update_component_req inReq = {};

    inReq.comp_classification = COMP_OTHER;
    inReq.comp_identifier = 0x01;
    inReq.comp_classification_index = 0x0F;
    inReq.comp_comparison_stamp = 0;
    inReq.comp_image_size = 32;
    inReq.update_option_flags.value = 1;
    inReq.comp_ver_str_type = PLDM_COMP_ASCII;
    inReq.comp_ver_str_len = compVerStrLen;

    std::fill(compVerStrArr.data(), compVerStrArr.end(), 0xFF);

    std::array<uint8_t, hdrSize + sizeof(struct pldm_update_component_req) +
                            compVerStrLen>
        outReq;

    auto msg = (struct pldm_msg*)outReq.data();

    auto rc = encode_update_component_req(
        instanceId, msg,
        sizeof(struct pldm_update_component_req) + inCompVerStr.length, &inReq,
        &inCompVerStr);

    auto request = (struct pldm_update_component_req*)(outReq.data() + hdrSize);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(msg->hdr.request, PLDM_REQUEST);
    EXPECT_EQ(msg->hdr.instance_id, instanceId);
    EXPECT_EQ(msg->hdr.type, PLDM_FWUP);
    EXPECT_EQ(msg->hdr.command, PLDM_UPDATE_COMPONENT);
    EXPECT_EQ(le16toh(request->comp_classification), inReq.comp_classification);
    EXPECT_EQ(le16toh(request->comp_identifier), inReq.comp_identifier);
    EXPECT_EQ(request->comp_classification_index,
              inReq.comp_classification_index);
    EXPECT_EQ(le32toh(request->comp_comparison_stamp),
              inReq.comp_comparison_stamp);
    EXPECT_EQ(le32toh(request->comp_image_size), inReq.comp_image_size);
    EXPECT_EQ(le32toh(request->update_option_flags.value),
              inReq.update_option_flags.value);
    EXPECT_EQ(request->comp_ver_str_type, inReq.comp_ver_str_type);
    EXPECT_EQ(request->comp_ver_str_len, inReq.comp_ver_str_len);
    EXPECT_EQ(true, std::equal(compVerStrArr.begin(), compVerStrArr.end(),
                               outReq.data() + hdrSize +
                                   sizeof(struct pldm_update_component_req)));
}

TEST(UpdateComponent, testBadEncodeRequest)
{
    uint8_t instanceId = 0x01;
    constexpr uint8_t compVerStrLen = 6;

    std::array<uint8_t, compVerStrLen> compVerStrArr;
    struct variable_field inCompVerStr;
    inCompVerStr.ptr = compVerStrArr.data();
    inCompVerStr.length = compVerStrLen;

    struct pldm_update_component_req inReq = {};

    std::fill(compVerStrArr.data(), compVerStrArr.end(), 0xFF);

    std::array<uint8_t, hdrSize + sizeof(struct pldm_update_component_req) +
                            compVerStrLen>
        outReq;

    auto msg = (struct pldm_msg*)outReq.data();

    auto rc = encode_update_component_req(
        instanceId, 0,
        sizeof(struct pldm_update_component_req) + inCompVerStr.length, &inReq,
        &inCompVerStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = encode_update_component_req(instanceId, msg, 0, &inReq, &inCompVerStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    rc = encode_update_component_req(instanceId, msg,
                                     sizeof(struct pldm_update_component_req) +
                                         inCompVerStr.length,
                                     NULL, &inCompVerStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = encode_update_component_req(instanceId, msg,
                                     sizeof(struct pldm_update_component_req) +
                                         inCompVerStr.length,
                                     &inReq, NULL);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inReq.comp_classification = COMP_UNKNOWN - 1;
    inReq.comp_identifier = 0x01;
    inReq.comp_classification_index = 0x0F;
    inReq.comp_comparison_stamp = 0;
    inReq.comp_image_size = 160;
    inReq.update_option_flags.value = 1;
    inReq.comp_ver_str_type = PLDM_COMP_VER_STR_TYPE_UNKNOWN - 1;
    inReq.comp_ver_str_len = 0;

    rc = encode_update_component_req(instanceId, msg,
                                     sizeof(struct pldm_update_component_req) +
                                         inCompVerStr.length,
                                     &inReq, &inCompVerStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inReq.comp_classification = 0x000F;
    inReq.comp_identifier = 0x00;
    inReq.comp_classification_index = 0x00;
    inReq.comp_comparison_stamp = 10;
    inReq.comp_image_size = 255;
    inReq.update_option_flags.value = 10;
    inReq.comp_ver_str_type = 7;
    inReq.comp_ver_str_len = 1;

    rc = encode_update_component_req(instanceId, msg,
                                     sizeof(struct pldm_update_component_req) +
                                         inCompVerStr.length,
                                     &inReq, &inCompVerStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inReq.comp_classification = COMP_SOFTWARE_BUNDLE + 1;
    inReq.comp_identifier = 0x01;
    inReq.comp_classification_index = 0x0F;
    inReq.comp_comparison_stamp = 0;
    inReq.comp_image_size = 161;
    inReq.update_option_flags.value = 1;
    inReq.comp_ver_str_type = PLDM_COMP_UTF_16BE + 1;
    inReq.comp_ver_str_len = 0;

    rc = encode_update_component_req(instanceId, msg,
                                     sizeof(struct pldm_update_component_req) +
                                         inCompVerStr.length,
                                     &inReq, &inCompVerStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inCompVerStr.ptr = NULL;
    inCompVerStr.length = 0;

    rc = encode_update_component_req(instanceId, msg,
                                     sizeof(struct pldm_update_component_req) +
                                         inCompVerStr.length,
                                     &inReq, &inCompVerStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(UpdateComponent, testGoodDecodeResponse)
{
    uint8_t completionCode = PLDM_ERROR;
    uint8_t compCompatabilityResp = COMPONENT_CANNOT_BE_UPDATED;
    uint8_t compCompatabilityRespCode = INVALID_COMP_COMPARISON_STAMP;
    bitfield32_t updateOptionFlagsEnabled = {1};
    uint16_t estimatedTimeReqFd = 1;

    std::array<uint8_t, hdrSize + sizeof(struct pldm_update_component_resp)>
        responseMsg{};
    struct pldm_update_component_resp* inResp =
        reinterpret_cast<struct pldm_update_component_resp*>(
            responseMsg.data() + hdrSize);
    inResp->completion_code = PLDM_SUCCESS;
    inResp->comp_compatability_resp = 1;
    inResp->comp_compatability_resp_code = 3;
    inResp->update_option_flags_enabled.value = 0x01;
    inResp->estimated_time_req_fd = 0x01;

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = decode_update_component_resp(
        response, responseMsg.size() - hdrSize, &completionCode,
        &compCompatabilityResp, &compCompatabilityRespCode,
        &updateOptionFlagsEnabled, &estimatedTimeReqFd);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(completionCode, PLDM_SUCCESS);
    EXPECT_EQ(compCompatabilityResp, inResp->comp_compatability_resp);
    EXPECT_EQ(compCompatabilityRespCode, inResp->comp_compatability_resp_code);
    EXPECT_EQ(updateOptionFlagsEnabled.value,
              htole32(inResp->update_option_flags_enabled.value));
    EXPECT_EQ(estimatedTimeReqFd, htole16(inResp->estimated_time_req_fd));
}

TEST(UpdateComponent, testBadDecodeResponse)
{
    uint8_t completionCode = PLDM_ERROR;
    uint8_t compCompatabilityResp = COMPONENT_CANNOT_BE_UPDATED;
    uint8_t compCompatabilityRespCode = INVALID_COMP_COMPARISON_STAMP;
    bitfield32_t updateOptionFlagsEnabled = {1};
    uint16_t estimatedTimeReqFd = 1;

    std::array<uint8_t, hdrSize + sizeof(struct pldm_update_component_resp)>
        responseMsg{};
    struct pldm_update_component_resp* inResp =
        reinterpret_cast<struct pldm_update_component_resp*>(
            responseMsg.data() + hdrSize);
    inResp->completion_code = PLDM_SUCCESS;
    inResp->comp_compatability_resp = 1;
    inResp->comp_compatability_resp_code = 3;
    inResp->update_option_flags_enabled.value = 0x01;
    inResp->estimated_time_req_fd = 0x01;

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = decode_update_component_resp(
        NULL, responseMsg.size() - hdrSize, &completionCode,
        &compCompatabilityResp, &compCompatabilityRespCode,
        &updateOptionFlagsEnabled, &estimatedTimeReqFd);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_update_component_resp(
        response, 0, &completionCode, &compCompatabilityResp,
        &compCompatabilityRespCode, &updateOptionFlagsEnabled,
        &estimatedTimeReqFd);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    rc = decode_update_component_resp(
        response, responseMsg.size() - hdrSize, NULL, &compCompatabilityResp,
        &compCompatabilityRespCode, &updateOptionFlagsEnabled,
        &estimatedTimeReqFd);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_update_component_resp(
        response, responseMsg.size() - hdrSize, &completionCode, NULL,
        &compCompatabilityRespCode, &updateOptionFlagsEnabled,
        &estimatedTimeReqFd);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_update_component_resp(response, responseMsg.size() - hdrSize,
                                      &completionCode, &compCompatabilityResp,
                                      NULL, &updateOptionFlagsEnabled,
                                      &estimatedTimeReqFd);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_update_component_resp(response, responseMsg.size() - hdrSize,
                                      &completionCode, &compCompatabilityResp,
                                      &compCompatabilityRespCode, NULL,
                                      &estimatedTimeReqFd);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_update_component_resp(response, responseMsg.size() - hdrSize,
                                      &completionCode, &compCompatabilityResp,
                                      &compCompatabilityRespCode,
                                      &updateOptionFlagsEnabled, NULL);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->comp_compatability_resp = COMPONENT_CAN_BE_UPDATED - 1;
    rc = decode_update_component_resp(
        response, responseMsg.size() - hdrSize, &completionCode,
        &compCompatabilityResp, &compCompatabilityRespCode,
        &updateOptionFlagsEnabled, &estimatedTimeReqFd);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->comp_compatability_resp = COMPONENT_CANNOT_BE_UPDATED + 1;
    rc = decode_update_component_resp(
        response, responseMsg.size() - hdrSize, &completionCode,
        &compCompatabilityResp, &compCompatabilityRespCode,
        &updateOptionFlagsEnabled, &estimatedTimeReqFd);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->comp_compatability_resp = 6;
    rc = decode_update_component_resp(
        response, responseMsg.size() - hdrSize, &completionCode,
        &compCompatabilityResp, &compCompatabilityRespCode,
        &updateOptionFlagsEnabled, &estimatedTimeReqFd);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->comp_compatability_resp_code =
        FD_VENDOR_COMP_STATUS_CODE_RANGE_MIN - 1;
    rc = decode_update_component_resp(
        response, responseMsg.size() - hdrSize, &completionCode,
        &compCompatabilityResp, &compCompatabilityRespCode,
        &updateOptionFlagsEnabled, &estimatedTimeReqFd);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->comp_compatability_resp_code =
        FD_VENDOR_COMP_STATUS_CODE_RANGE_MAX + 1;
    rc = decode_update_component_resp(
        response, responseMsg.size() - hdrSize, &completionCode,
        &compCompatabilityResp, &compCompatabilityRespCode,
        &updateOptionFlagsEnabled, &estimatedTimeReqFd);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->comp_compatability_resp_code = COMP_CAN_BE_UPDATED - 1;
    rc = decode_update_component_resp(
        response, responseMsg.size() - hdrSize, &completionCode,
        &compCompatabilityResp, &compCompatabilityRespCode,
        &updateOptionFlagsEnabled, &estimatedTimeReqFd);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->comp_compatability_resp_code = 0xFF;
    rc = decode_update_component_resp(
        response, responseMsg.size() - hdrSize, &completionCode,
        &compCompatabilityResp, &compCompatabilityRespCode,
        &updateOptionFlagsEnabled, &estimatedTimeReqFd);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(GetStatus, testGoodEncodeRequest)
{
    uint8_t instanceId = 0x01;
    struct pldm_msg msg;
    auto rc = encode_get_status_req(instanceId, &msg);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(msg.hdr.instance_id, instanceId);
    EXPECT_EQ(msg.hdr.type, PLDM_FWUP);
    EXPECT_EQ(msg.hdr.request, PLDM_REQUEST);
    EXPECT_EQ(msg.hdr.command, PLDM_GET_STATUS);
}

TEST(GetStatus, testBadEncodeRequest)
{
    uint8_t instanceId = 0x01;
    auto rc = encode_get_status_req(instanceId, NULL);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(GetStatus, testGoodDecodeResponse)
{
    uint8_t completionCode = PLDM_ERROR;
    uint8_t currentState = 0;
    uint8_t previousState = 0;
    uint8_t auxState = 0;
    uint8_t auxStateStatus = 0;
    uint8_t progressPercent = 0;
    uint8_t reasonCode = 0;
    bitfield32_t updateOptionFlagsEnabled = {0};

    std::array<uint8_t, hdrSize + sizeof(struct pldm_get_status_resp)>
        responseMsg{};

    struct pldm_get_status_resp* inResp =
        reinterpret_cast<struct pldm_get_status_resp*>(responseMsg.data() +
                                                       hdrSize);

    inResp->aux_state = FD_OPERATION_IN_PROGRESS;
    inResp->aux_state_status = FD_AUX_STATE_IN_PROGRESS_OR_SUCCESS;
    inResp->current_state = FD_IDLE;
    inResp->previous_state = FD_IDLE;
    inResp->progress_percent = 0x00;
    inResp->reason_code = FD_INITIALIZATION;
    inResp->update_option_flags_enabled.value = 0;

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    response->hdr.command = PLDM_GET_STATUS;
    response->hdr.request = 0b0;
    response->hdr.datagram = 0b0;
    response->hdr.type = PLDM_FWUP;
    response->payload[0] = PLDM_SUCCESS;

    auto rc = decode_get_status_resp(
        response, responseMsg.size() - hdrSize, &completionCode, &currentState,
        &previousState, &auxState, &auxStateStatus, &progressPercent,
        &reasonCode, &updateOptionFlagsEnabled);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(completionCode, PLDM_SUCCESS);
    EXPECT_EQ(currentState, inResp->current_state);
    EXPECT_EQ(previousState, inResp->previous_state);
    EXPECT_EQ(auxState, inResp->aux_state);
    EXPECT_EQ(auxStateStatus, inResp->aux_state_status);
    EXPECT_EQ(progressPercent, inResp->progress_percent);
    EXPECT_EQ(reasonCode, inResp->reason_code);
    EXPECT_EQ(updateOptionFlagsEnabled.value,
              htole32(inResp->update_option_flags_enabled.value));
}

TEST(GetStatus, testBadDecodeResponse)
{
    uint8_t completionCode = PLDM_ERROR;
    uint8_t currentState = false;
    uint8_t previousState = false;
    uint8_t auxState = false;
    uint8_t auxStateStatus = false;
    uint8_t progressPercent = 0;
    uint8_t reasonCode = false;
    bitfield32_t updateOptionFlagsEnabled = {0};

    std::array<uint8_t, hdrSize + sizeof(struct pldm_get_status_resp)>
        responseMsg{};

    struct pldm_get_status_resp* inResp =
        reinterpret_cast<struct pldm_get_status_resp*>(responseMsg.data() +
                                                       hdrSize);

    inResp->aux_state = FD_OPERATION_FAILED;
    inResp->aux_state_status = FD_GENERIC_ERROR;
    inResp->current_state = FD_LEARN_COMPONENTS;
    inResp->previous_state = FD_IDLE;
    inResp->progress_percent = 0x44;
    inResp->reason_code = FD_TIMEOUT_LEARN_COMPONENT;
    inResp->update_option_flags_enabled.value = 1;

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    response->hdr.command = PLDM_GET_STATUS;
    response->hdr.request = 0b0;
    response->hdr.datagram = 0b0;
    response->hdr.type = PLDM_FWUP;
    response->payload[0] = PLDM_ERROR_INVALID_DATA;

    auto rc = decode_get_status_resp(
        response, responseMsg.size() - hdrSize, &completionCode, &currentState,
        &previousState, &auxState, &auxStateStatus, &progressPercent,
        &reasonCode, &updateOptionFlagsEnabled);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    response->payload[0] = PLDM_SUCCESS;
    inResp->aux_state = 4;
    inResp->aux_state_status = 0x05;
    inResp->current_state = 9;
    inResp->previous_state = 10;
    inResp->progress_percent = 0x44;
    inResp->reason_code = 8;
    inResp->update_option_flags_enabled.value = 1;

    rc = decode_get_status_resp(response, responseMsg.size() - hdrSize,
                                &completionCode, &currentState, &previousState,
                                &auxState, &auxStateStatus, &progressPercent,
                                &reasonCode, &updateOptionFlagsEnabled);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_status_resp(NULL, responseMsg.size() - hdrSize,
                                &completionCode, &currentState, &previousState,
                                &auxState, &auxStateStatus, &progressPercent,
                                &reasonCode, &updateOptionFlagsEnabled);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_status_resp(response, 0, &completionCode, &currentState,
                                &previousState, &auxState, &auxStateStatus,
                                &progressPercent, &reasonCode,
                                &updateOptionFlagsEnabled);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    rc = decode_get_status_resp(response, responseMsg.size() - hdrSize, NULL,
                                &currentState, &previousState, &auxState,
                                &auxStateStatus, &progressPercent, &reasonCode,
                                &updateOptionFlagsEnabled);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_status_resp(response, responseMsg.size() - hdrSize,
                                &completionCode, NULL, &previousState,
                                &auxState, &auxStateStatus, &progressPercent,
                                &reasonCode, &updateOptionFlagsEnabled);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_status_resp(response, responseMsg.size() - hdrSize,
                                &completionCode, &currentState, NULL, &auxState,
                                &auxStateStatus, &progressPercent, &reasonCode,
                                &updateOptionFlagsEnabled);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_status_resp(response, responseMsg.size() - hdrSize,
                                &completionCode, &currentState, &previousState,
                                NULL, &auxStateStatus, &progressPercent,
                                &reasonCode, &updateOptionFlagsEnabled);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_status_resp(response, responseMsg.size() - hdrSize,
                                &completionCode, &currentState, &previousState,
                                &auxState, NULL, &progressPercent, &reasonCode,
                                &updateOptionFlagsEnabled);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_status_resp(response, responseMsg.size() - hdrSize,
                                &completionCode, &currentState, &previousState,
                                &auxState, &auxStateStatus, NULL, &reasonCode,
                                &updateOptionFlagsEnabled);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_status_resp(response, responseMsg.size() - hdrSize,
                                &completionCode, &currentState, &previousState,
                                &auxState, &auxStateStatus, &progressPercent,
                                NULL, &updateOptionFlagsEnabled);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_status_resp(response, responseMsg.size() - hdrSize,
                                &completionCode, &currentState, &previousState,
                                &auxState, &auxStateStatus, &progressPercent,
                                &reasonCode, NULL);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->aux_state = FD_OPERATION_IN_PROGRESS - 1;
    inResp->aux_state_status = FD_AUX_STATE_IN_PROGRESS_OR_SUCCESS - 1;
    inResp->current_state = FD_IDLE - 1;
    inResp->previous_state = FD_IDLE - 1;
    inResp->progress_percent = 0x66;
    inResp->reason_code = FD_INITIALIZATION - 1;
    rc = decode_get_status_resp(response, responseMsg.size() - hdrSize,
                                &completionCode, &currentState, &previousState,
                                &auxState, &auxStateStatus, &progressPercent,
                                &reasonCode, &updateOptionFlagsEnabled);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->aux_state = FD_WAIT + 1;
    inResp->aux_state_status = FD_AUX_STATE_IN_PROGRESS_OR_SUCCESS + 1;
    inResp->current_state = FD_ACTIVATE + 1;
    inResp->previous_state = FD_ACTIVATE + 1;
    inResp->progress_percent = 0x82;
    inResp->reason_code = FD_TIMEOUT_DOWNLOAD + 1;
    rc = decode_get_status_resp(response, responseMsg.size() - hdrSize,
                                &completionCode, &currentState, &previousState,
                                &auxState, &auxStateStatus, &progressPercent,
                                &reasonCode, &updateOptionFlagsEnabled);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->aux_state_status = FD_VENDOR_DEFINED_STATUS_CODE_END + 1;
    inResp->progress_percent = 0xFF;
    rc = decode_get_status_resp(response, responseMsg.size() - hdrSize,
                                &completionCode, &currentState, &previousState,
                                &auxState, &auxStateStatus, &progressPercent,
                                &reasonCode, &updateOptionFlagsEnabled);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->aux_state_status = FD_VENDOR_DEFINED_STATUS_CODE_START - 1;
    inResp->reason_code = FD_STATUS_VENDOR_DEFINED_MIN - 1;
    rc = decode_get_status_resp(response, responseMsg.size() - hdrSize,
                                &completionCode, &currentState, &previousState,
                                &auxState, &auxStateStatus, &progressPercent,
                                &reasonCode, &updateOptionFlagsEnabled);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->aux_state_status = FD_TIMEOUT - 1;
    rc = decode_get_status_resp(response, responseMsg.size() - hdrSize,
                                &completionCode, &currentState, &previousState,
                                &auxState, &auxStateStatus, &progressPercent,
                                &reasonCode, &updateOptionFlagsEnabled);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->aux_state_status = FD_GENERIC_ERROR + 1;
    rc = decode_get_status_resp(response, responseMsg.size() - hdrSize,
                                &completionCode, &currentState, &previousState,
                                &auxState, &auxStateStatus, &progressPercent,
                                &reasonCode, &updateOptionFlagsEnabled);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(CancelUpdate, testGoodEncodeRequest)
{
    uint8_t instanceId = 0x03;
    struct pldm_msg msg;
    auto rc = encode_cancel_update_req(instanceId, &msg);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(msg.hdr.instance_id, instanceId);
    EXPECT_EQ(msg.hdr.type, PLDM_FWUP);
    EXPECT_EQ(msg.hdr.request, PLDM_REQUEST);
    EXPECT_EQ(msg.hdr.command, PLDM_CANCEL_UPDATE);
}

TEST(CancelUpdate, testBadEncodeRequest)
{
    uint8_t instanceId = 0x03;
    auto rc = encode_cancel_update_req(instanceId, NULL);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(CancelUpdate, testGoodDecodeResponse)
{
    uint8_t completionCode = PLDM_ERROR;
    bool8_t nonFunctioningComponentIndication = COMPONENTS_FUNCTIONING;
    bitfield64_t nonFunctioningComponentBitmap = {1};

    std::array<uint8_t, hdrSize + sizeof(struct pldm_cancel_update_resp)>
        responseMsg{};

    struct pldm_cancel_update_resp* inResp =
        reinterpret_cast<struct pldm_cancel_update_resp*>(responseMsg.data() +
                                                          hdrSize);

    inResp->completion_code = PLDM_SUCCESS;
    inResp->non_functioning_component_indication = 0;
    inResp->non_functioning_component_bitmap = 1;

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    response->hdr.command = PLDM_CANCEL_UPDATE;
    response->hdr.request = 0b0;
    response->hdr.datagram = 0b0;
    response->hdr.type = PLDM_FWUP;
    response->payload[0] = PLDM_SUCCESS;

    auto rc = decode_cancel_update_resp(
        response, responseMsg.size() - hdrSize, &completionCode,
        &nonFunctioningComponentIndication, &nonFunctioningComponentBitmap);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(completionCode, PLDM_SUCCESS);
    EXPECT_EQ(nonFunctioningComponentIndication,
              inResp->non_functioning_component_indication);
    EXPECT_EQ(nonFunctioningComponentBitmap.value,
              htole64(inResp->non_functioning_component_bitmap));
    inResp->non_functioning_component_indication = COMPONENTS_FUNCTIONING;
    nonFunctioningComponentBitmap = {0x5};
    rc = decode_cancel_update_resp(
        response, responseMsg.size() - hdrSize, &completionCode,
        &nonFunctioningComponentIndication, &nonFunctioningComponentBitmap);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(nonFunctioningComponentIndication,
              inResp->non_functioning_component_indication);
    EXPECT_EQ(nonFunctioningComponentBitmap.value, 0x5);
}

TEST(CancelUpdate, testBadDecodeResponse)
{
    uint8_t completionCode = PLDM_ERROR;
    bool8_t nonFunctioningComponentIndication = COMPONENTS_NOT_FUNCTIONING;
    bitfield64_t nonFunctioningComponentBitmap = {1};

    std::array<uint8_t, hdrSize + sizeof(struct pldm_cancel_update_resp)>
        responseMsg{};

    struct pldm_cancel_update_resp* inResp =
        reinterpret_cast<struct pldm_cancel_update_resp*>(responseMsg.data() +
                                                          hdrSize);

    inResp->completion_code = PLDM_SUCCESS;
    inResp->non_functioning_component_indication = 1;
    inResp->non_functioning_component_bitmap = 1;

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    response->hdr.command = PLDM_CANCEL_UPDATE;
    response->hdr.request = 0b0;
    response->hdr.datagram = 0b0;
    response->hdr.type = PLDM_FWUP;
    response->payload[0] = PLDM_SUCCESS;

    auto rc = decode_cancel_update_resp(response, 0, &completionCode,
                                        &nonFunctioningComponentIndication,
                                        &nonFunctioningComponentBitmap);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    response->payload[0] = PLDM_ERROR_INVALID_DATA;
    rc = decode_cancel_update_resp(
        response, responseMsg.size() - hdrSize, &completionCode,
        &nonFunctioningComponentIndication, &nonFunctioningComponentBitmap);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_cancel_update_resp(
        NULL, responseMsg.size() - hdrSize, &completionCode,
        &nonFunctioningComponentIndication, &nonFunctioningComponentBitmap);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_cancel_update_resp(response, responseMsg.size() - hdrSize, NULL,
                                   &nonFunctioningComponentIndication,
                                   &nonFunctioningComponentBitmap);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_cancel_update_resp(response, responseMsg.size() - hdrSize,
                                   &completionCode, NULL,
                                   &nonFunctioningComponentBitmap);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_cancel_update_resp(response, responseMsg.size() - hdrSize,
                                   &completionCode,
                                   &nonFunctioningComponentIndication, NULL);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    response->payload[0] = PLDM_SUCCESS;
    inResp->non_functioning_component_indication = COMPONENTS_FUNCTIONING - 1;
    rc = decode_cancel_update_resp(
        response, responseMsg.size() - hdrSize, &completionCode,
        &nonFunctioningComponentIndication, &nonFunctioningComponentBitmap);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->non_functioning_component_indication =
        COMPONENTS_NOT_FUNCTIONING + 1;
    rc = decode_cancel_update_resp(
        response, responseMsg.size() - hdrSize, &completionCode,
        &nonFunctioningComponentIndication, &nonFunctioningComponentBitmap);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    inResp->non_functioning_component_indication = 0x0F;
    rc = decode_cancel_update_resp(
        response, responseMsg.size() - hdrSize, &completionCode,
        &nonFunctioningComponentIndication, &nonFunctioningComponentBitmap);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(CancelUpdateComponent, testGoodEncodeRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr)> requestMsg{};
    auto requestPtr = reinterpret_cast<pldm_msg*>(requestMsg.data());

    uint8_t instanceId = 0x01;

    auto rc = encode_cancel_update_component_req(instanceId, requestPtr);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(requestPtr->hdr.request, PLDM_REQUEST);
    EXPECT_EQ(requestPtr->hdr.instance_id, instanceId);
    EXPECT_EQ(requestPtr->hdr.type, PLDM_FWUP);
    EXPECT_EQ(requestPtr->hdr.command, PLDM_CANCEL_UPDATE_COMPONENT);
}

TEST(CancelUpdateComponent, testBadEncodeRequest)
{
    uint8_t instanceId = 0x01;

    auto rc = encode_cancel_update_component_req(instanceId, NULL);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(CancelUpdateComponent, testGoodDecodeResponse)
{
    uint8_t completionCode = PLDM_ERROR;

    std::array<uint8_t, hdrSize + sizeof(uint8_t)> responseMsg{};

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = decode_cancel_update_component_resp(
        response, responseMsg.size() - hdrSize, &completionCode);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(completionCode, PLDM_SUCCESS);
}

TEST(CancelUpdateComponent, testBadDecodeResponse)
{
    uint8_t completionCode = PLDM_ERROR;

    std::array<uint8_t, hdrSize + sizeof(uint8_t)> responseMsg{};

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = decode_cancel_update_component_resp(
        NULL, responseMsg.size() - hdrSize, &completionCode);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_cancel_update_component_resp(response, 0, &completionCode);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    rc = decode_cancel_update_component_resp(
        response, responseMsg.size() - hdrSize, NULL);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(ActivateFirmware, testGoodEncodeRequest)
{
    std::array<uint8_t, hdrSize + sizeof(struct pldm_activate_firmware_req)>
        requestMsg{};

    auto msg = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto request = reinterpret_cast<pldm_activate_firmware_req*>(msg->payload);

    bool8_t selfContainedActivationReq = CONTAINS_SELF_ACTIVATED_COMPONENTS;

    auto rc = encode_activate_firmware_req(
        0, msg, sizeof(struct pldm_activate_firmware_req),
        selfContainedActivationReq);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(msg->hdr.request, PLDM_REQUEST);
    EXPECT_EQ(msg->hdr.instance_id, 0u);
    EXPECT_EQ(msg->hdr.type, PLDM_FWUP);
    EXPECT_EQ(msg->hdr.command, PLDM_ACTIVATE_FIRMWARE);
    EXPECT_EQ(selfContainedActivationReq,
              request->self_contained_activation_req);
}

TEST(ActivateFirmware, testBadEncodeRequest)
{
    std::array<uint8_t, hdrSize + sizeof(struct pldm_activate_firmware_req)>
        requestMsg{};

    auto msg = reinterpret_cast<pldm_msg*>(requestMsg.data());

    bool8_t selfContainedActivationReq =
        NOT_CONTAINING_SELF_ACTIVATED_COMPONENTS;

    auto rc = encode_activate_firmware_req(
        0, 0, sizeof(struct pldm_activate_firmware_req),
        selfContainedActivationReq);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = encode_activate_firmware_req(0, msg, 0, selfContainedActivationReq);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    rc = encode_activate_firmware_req(
        0, 0, sizeof(struct pldm_activate_firmware_req), 0);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    selfContainedActivationReq = CONTAINS_SELF_ACTIVATED_COMPONENTS + 1;
    rc = encode_activate_firmware_req(0, msg,
                                      sizeof(struct pldm_activate_firmware_req),
                                      selfContainedActivationReq);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    selfContainedActivationReq = NOT_CONTAINING_SELF_ACTIVATED_COMPONENTS - 1;
    rc = encode_activate_firmware_req(0, msg,
                                      sizeof(struct pldm_activate_firmware_req),
                                      selfContainedActivationReq);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    selfContainedActivationReq = 6;
    rc = encode_activate_firmware_req(0, msg,
                                      sizeof(struct pldm_activate_firmware_req),
                                      selfContainedActivationReq);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(ActivateFirmware, testGoodDecodeResponse)
{
    uint8_t completionCode = PLDM_ERROR;
    uint16_t estimatedTimeActivation = 1;

    std::array<uint8_t, hdrSize + sizeof(struct pldm_activate_firmware_resp)>
        responseMsg{};
    struct pldm_activate_firmware_resp* inResp =
        reinterpret_cast<struct pldm_activate_firmware_resp*>(
            responseMsg.data() + hdrSize);
    inResp->completion_code = PLDM_SUCCESS;
    inResp->estimated_time_activation = 0x01;

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = decode_activate_firmware_resp(
        response, responseMsg.size() - hdrSize, &completionCode,
        &estimatedTimeActivation);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(completionCode, PLDM_SUCCESS);
    EXPECT_EQ(estimatedTimeActivation,
              htole16(inResp->estimated_time_activation));
}

TEST(ActivateFirmware, testBadDecodeResponse)
{
    uint8_t completionCode = PLDM_ERROR;
    uint16_t estimatedTimeActivation = 0;

    std::array<uint8_t, hdrSize + sizeof(struct pldm_activate_firmware_resp)>
        responseMsg{};
    struct pldm_activate_firmware_resp* inResp =
        reinterpret_cast<struct pldm_activate_firmware_resp*>(
            responseMsg.data() + hdrSize);
    inResp->completion_code = PLDM_SUCCESS;
    inResp->estimated_time_activation = 0x00;

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    auto rc = decode_activate_firmware_resp(NULL, responseMsg.size() - hdrSize,
                                            &completionCode,
                                            &estimatedTimeActivation);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_activate_firmware_resp(response, 0, &completionCode,
                                       &estimatedTimeActivation);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    rc = decode_activate_firmware_resp(response, responseMsg.size() - hdrSize,
                                       NULL, &estimatedTimeActivation);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_activate_firmware_resp(response, responseMsg.size() - hdrSize,
                                       &completionCode, NULL);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(RequestFirmwareData, testGoodDecodeRequest)
{
    uint32_t offset;
    uint32_t length;
    std::array<uint8_t, hdrSize + sizeof(struct request_firmware_data_req)>
        requestMsg{};
    struct request_firmware_data_req* request =
        reinterpret_cast<struct request_firmware_data_req*>(requestMsg.data() +
                                                            hdrSize);
    request->length = 64;
    request->offset = 0;
    auto requestIn = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = decode_request_firmware_data_req(
        requestIn, requestMsg.size() - hdrSize, &offset, &length);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(offset, request->offset);
    EXPECT_EQ(length, request->length);
}

TEST(RequestFirmwareData, testBadDecodeRequest)
{
    uint32_t offset;
    uint32_t length;
    std::array<uint8_t, hdrSize + sizeof(struct request_firmware_data_req)>
        requestMsg{};
    struct request_firmware_data_req* request =
        reinterpret_cast<struct request_firmware_data_req*>(requestMsg.data() +
                                                            hdrSize);
    request->length = 64;
    request->offset = 0;
    auto requestIn = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = decode_request_firmware_data_req(
        NULL, requestMsg.size() - hdrSize, &offset, &length);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    rc = decode_request_firmware_data_req(requestIn, 0, &offset, &length);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
    rc = decode_request_firmware_data_req(
        requestIn, requestMsg.size() - hdrSize, NULL, &length);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    rc = decode_request_firmware_data_req(
        requestIn, requestMsg.size() - hdrSize, &offset, NULL);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(RequestFirmwareData, testGoodEncodeResponse)
{
    // ComponentImagePortion is not fixed, here taking it as 64 byte
    constexpr uint8_t compImgLen = 64;
    std::array<uint8_t, compImgLen> image;
    size_t payload_length = compImgLen + 1;
    struct variable_field img;
    img.ptr = image.data();
    img.length = 32;
    uint8_t instanceID = 0x01;
    uint8_t completionCode = PLDM_SUCCESS;
    std::fill(image.data(), image.end(), 0xFF);
    std::array<uint8_t, hdrSize + 1 + compImgLen> resp;
    auto msg = (struct pldm_msg*)resp.data();
    auto rc = encode_request_firmware_data_resp(instanceID, msg, payload_length,
                                                completionCode, &img);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(msg->hdr.instance_id, instanceID);
    EXPECT_EQ(msg->hdr.type, PLDM_FWUP);
    EXPECT_EQ(msg->hdr.command, PLDM_REQUEST_FIRMWARE_DATA);
    EXPECT_EQ(msg->payload[0], completionCode);
    EXPECT_EQ(0, memcmp(img.ptr, msg->payload + 1, img.length));
}

TEST(RequestFirmwareData, testBadEncodeResponse)
{
    // ComponentImagePortion is not fixed, here taking it as 64 byte
    constexpr uint8_t compImgLen = 64;
    std::array<uint8_t, compImgLen> image;
    size_t payload_length = compImgLen + 1;
    struct variable_field img;
    img.ptr = image.data();
    img.length = 32;
    uint8_t instanceID = 0x01;
    uint8_t completionCode = PLDM_SUCCESS;
    std::fill(image.data(), image.end(), 0xFF);
    std::array<uint8_t, hdrSize + 1 + compImgLen> resp;
    auto msg = (struct pldm_msg*)resp.data();
    auto rc = encode_request_firmware_data_resp(
        instanceID, NULL, payload_length, completionCode, &img);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    rc = encode_request_firmware_data_resp(instanceID, msg, payload_length,
                                           completionCode, NULL);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    img.length = 0;
    rc = encode_request_firmware_data_resp(instanceID, msg, payload_length,
                                           completionCode, &img);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
    img.ptr = NULL;
    rc = encode_request_firmware_data_resp(instanceID, msg, payload_length,
                                           completionCode, &img);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}
