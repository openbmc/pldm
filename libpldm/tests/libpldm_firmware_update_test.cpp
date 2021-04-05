#include "libpldm/base.h"
#include "libpldm/firmware_update.h"

#include <gtest/gtest.h>

constexpr auto hdrSize = sizeof(pldm_msg_hdr);
TEST(QueryDeviceIdentifiers, testGoodEncodeRequest)
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
TEST(QueryDeviceIdentifiers, testGoodDecodeResponse)
{
    uint8_t completionCode = PLDM_SUCCESS;
    uint32_t deviceIdentifiersLen = 0;
    uint8_t descriptorCount = 0;
    uint8_t* outDescriptorData = nullptr;
    // descriptorDataLen is not fixed here taking it as 6
    constexpr uint8_t descriptorDataLen = 6;

    std::array<uint8_t, hdrSize + sizeof(struct query_device_identifiers_resp) +
                            descriptorDataLen>
        responseMsg{};
    auto inResp = reinterpret_cast<struct query_device_identifiers_resp*>(
        responseMsg.data() + hdrSize);

    inResp->completion_code = PLDM_SUCCESS;
    inResp->device_identifiers_len = descriptorDataLen;
    inResp->descriptor_count = 1;

    // filling descriptor data
    std::fill(responseMsg.data() + hdrSize +
                  sizeof(struct query_device_identifiers_resp),
              responseMsg.end() - 1, 0xFF);

    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());

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
                         responseMsg.data() + hdrSize +
                             sizeof(struct query_device_identifiers_resp)));
}
TEST(GetFirmwareParameters, testGoodEncodeRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr)> requestMsg{};
    auto requestPtr = reinterpret_cast<pldm_msg*>(requestMsg.data());
    uint8_t instanceId = 0x01;

    auto rc = encode_get_firmware_parameters_req(
        instanceId, requestPtr, PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(requestPtr->hdr.request, PLDM_REQUEST);
    EXPECT_EQ(requestPtr->hdr.instance_id, instanceId);
    EXPECT_EQ(requestPtr->hdr.type, PLDM_FWUP);
    EXPECT_EQ(requestPtr->hdr.command, PLDM_GET_FIRMWARE_PARAMETERS);
}

TEST(GetFWParams, testGoodDecodeCompImgSetResponse)
{
    // ActiveCompImageSetVerStrLen is not fixed here taking it as 8
    constexpr uint8_t activeCompImageSetVerStrLen = 8;
    // PendingCompImageSetVerStrLen is not fixed here taking it as 8
    constexpr uint8_t pendingCompImageSetVerStrLen = 8;
    uint8_t completionCode = PLDM_SUCCESS;
    constexpr size_t payloadLen = sizeof(struct get_firmware_parameters_resp) +
                                  activeCompImageSetVerStrLen +
                                  pendingCompImageSetVerStrLen;

    std::array<uint8_t, hdrSize + payloadLen> response{};
    struct get_firmware_parameters_resp* inResp =
        reinterpret_cast<struct get_firmware_parameters_resp*>(response.data() +
                                                               hdrSize);
    inResp->completion_code = PLDM_SUCCESS;
    inResp->capabilities_during_update.value = 0x0F;
    inResp->comp_count = 1;
    inResp->active_comp_image_set_ver_str_type = 1;
    inResp->active_comp_image_set_ver_str_len = activeCompImageSetVerStrLen;
    inResp->pending_comp_image_set_ver_str_type = 1;
    inResp->pending_comp_image_set_ver_str_len = pendingCompImageSetVerStrLen;

    constexpr uint32_t activeCompImageSetVerStrIndex =
        hdrSize + sizeof(struct get_firmware_parameters_resp);
    // filling default values for ActiveComponentImageSetVersionString
    std::fill_n(response.data() + activeCompImageSetVerStrIndex,
                activeCompImageSetVerStrLen, 0xFF);

    constexpr uint32_t pendingCompImageSetVerStrIndex =
        hdrSize + sizeof(struct get_firmware_parameters_resp) +
        activeCompImageSetVerStrLen;
    // filling default values for ActiveComponentImageSetVersionString
    std::fill_n(response.data() + pendingCompImageSetVerStrIndex,
                pendingCompImageSetVerStrLen, 0xFF);

    auto responseMsg = reinterpret_cast<pldm_msg*>(response.data());

    struct get_firmware_parameters_resp outResp;
    struct variable_field outActiveCompImageSetVerStr;
    struct variable_field outPendingCompImageSetVerStr;

    auto rc = decode_get_firmware_parameters_comp_img_set_resp(
        responseMsg, payloadLen, &completionCode, &outResp,
        &outActiveCompImageSetVerStr, &outPendingCompImageSetVerStr);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(inResp->completion_code, PLDM_SUCCESS);

    EXPECT_EQ(inResp->capabilities_during_update.value,
              outResp.capabilities_during_update.value);
    EXPECT_EQ(inResp->comp_count, outResp.comp_count);
    EXPECT_EQ(inResp->active_comp_image_set_ver_str_type,
              outResp.active_comp_image_set_ver_str_type);
    EXPECT_EQ(inResp->active_comp_image_set_ver_str_len,
              outResp.active_comp_image_set_ver_str_len);
    EXPECT_EQ(0, memcmp(outActiveCompImageSetVerStr.ptr,
                        response.data() + activeCompImageSetVerStrIndex,
                        outActiveCompImageSetVerStr.length));

    EXPECT_EQ(inResp->pending_comp_image_set_ver_str_type,
              outResp.pending_comp_image_set_ver_str_type);
    EXPECT_EQ(inResp->pending_comp_image_set_ver_str_len,
              outResp.pending_comp_image_set_ver_str_len);
    EXPECT_EQ(0, memcmp(outPendingCompImageSetVerStr.ptr,
                        response.data() + pendingCompImageSetVerStrIndex,
                        outPendingCompImageSetVerStr.length));
}
TEST(GetFWParams, testGoodDecodeCompResponse)
{
    // ActiveCompImageSetVerStrLen is not fixed here taking it as 8
    constexpr uint8_t activeCompVerStrLen = 8;
    // PendingCompImageSetVerStrLen is not fixed here taking it as 8
    constexpr uint8_t pendingCompVerStrLen = 8;

    constexpr size_t payloadLen = sizeof(struct component_parameter_table) +
                                  activeCompVerStrLen + pendingCompVerStrLen;

    std::array<uint8_t, payloadLen> response{};

    struct component_parameter_table* inCompData =
        reinterpret_cast<struct component_parameter_table*>(response.data());

    inCompData->comp_classification = 0x0F;
    inCompData->comp_identifier = 0x01;
    inCompData->comp_classification_index = 0x0F;
    inCompData->active_comp_comparison_stamp = 0;
    inCompData->active_comp_ver_str_type = 1;
    inCompData->active_comp_ver_str_len = activeCompVerStrLen;
    std::fill_n(inCompData->active_comp_release_date,
                sizeof(inCompData->active_comp_release_date), 0xFF);
    inCompData->pending_comp_comparison_stamp = 0;
    inCompData->pending_comp_ver_str_type = 1;
    inCompData->pending_comp_ver_str_len = pendingCompVerStrLen;

    std::fill_n(inCompData->pending_comp_release_date,
                sizeof(inCompData->pending_comp_release_date), 0xFF);
    inCompData->comp_activation_methods.value = 0x0F;
    inCompData->capabilities_during_update.value = 0x0F;

    constexpr uint32_t activeCompVerStrIndex =
        sizeof(struct component_parameter_table);
    std::fill_n(response.data() + activeCompVerStrIndex, activeCompVerStrLen,
                0xFF);

    constexpr uint32_t pendingCompVerStrIndex =
        activeCompVerStrIndex + activeCompVerStrLen;
    std::fill_n(response.data() + pendingCompVerStrIndex, pendingCompVerStrLen,
                0xFF);

    struct component_parameter_table outCompData;
    struct variable_field outActiveCompVerStr;
    struct variable_field outPendingCompVerStr;

    auto rc = decode_get_firmware_parameters_comp_resp(
        response.data(), payloadLen, &outCompData, &outActiveCompVerStr,
        &outPendingCompVerStr);

    EXPECT_EQ(rc, PLDM_SUCCESS);

    EXPECT_EQ(inCompData->comp_classification, outCompData.comp_classification);
    EXPECT_EQ(inCompData->comp_identifier, outCompData.comp_identifier);
    EXPECT_EQ(inCompData->comp_classification_index,
              outCompData.comp_classification_index);
    EXPECT_EQ(inCompData->active_comp_comparison_stamp,
              outCompData.active_comp_comparison_stamp);
    EXPECT_EQ(inCompData->active_comp_ver_str_type,
              outCompData.active_comp_ver_str_type);
    EXPECT_EQ(inCompData->active_comp_ver_str_len,
              outCompData.active_comp_ver_str_len);
    EXPECT_EQ(0, memcmp(inCompData->active_comp_release_date,
                        outCompData.active_comp_release_date,
                        sizeof(inCompData->active_comp_release_date)));
    EXPECT_EQ(inCompData->pending_comp_comparison_stamp,
              outCompData.pending_comp_comparison_stamp);
    EXPECT_EQ(inCompData->pending_comp_ver_str_type,
              outCompData.pending_comp_ver_str_type);
    EXPECT_EQ(inCompData->pending_comp_ver_str_len,
              outCompData.pending_comp_ver_str_len);
    EXPECT_EQ(0, memcmp(inCompData->pending_comp_release_date,
                        outCompData.pending_comp_release_date,
                        sizeof(inCompData->pending_comp_release_date)));
    EXPECT_EQ(inCompData->comp_activation_methods.value,
              outCompData.comp_activation_methods.value);
    EXPECT_EQ(inCompData->capabilities_during_update.value,
              outCompData.capabilities_during_update.value);

    EXPECT_EQ(0, memcmp(outActiveCompVerStr.ptr,
                        response.data() + activeCompVerStrIndex,
                        outActiveCompVerStr.length));

    EXPECT_EQ(0, memcmp(outPendingCompVerStr.ptr,
                        response.data() + pendingCompVerStrIndex,
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
