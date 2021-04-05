#include "../base.h"
#include "../firmware_update.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

constexpr auto hdrSize = sizeof(pldm_msg_hdr);
TEST(QueryDeviceIdentifiers, testGoodEncodeRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr)> requestMsg{};
    auto requestPtr = reinterpret_cast<pldm_msg*>(requestMsg.data());

    uint8_t instanceId = 0x01;

    auto rc = encode_query_device_identifiers_req(
        instanceId, requestPtr, PLDM_QUERY_DEVICE_IDENTIFIERS_REQ_BYTES);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(requestPtr->hdr.request, PLDM_REQUEST);
    EXPECT_EQ(requestPtr->hdr.instance_id, instanceId);
    EXPECT_EQ(requestPtr->hdr.type, PLDM_FWU);
    EXPECT_EQ(requestPtr->hdr.command, PLDM_QUERY_DEVICE_IDENTIFIERS);
}
TEST(QueryDeviceIdentifiers, testGoodDecodeResponse)
{
    uint8_t completionCode = PLDM_SUCCESS;
    uint32_t deviceIdentifiersLen = 0;
    uint8_t descriptorCount = 0;
    // descriptorDataLen is not fixed here taking it as 6
    constexpr uint8_t descriptorDataLen = 6;
    std::array<uint8_t, descriptorDataLen> descriptorData;
    struct variable_field outDescriptorData;
    outDescriptorData.length = descriptorData.size();
    outDescriptorData.ptr = descriptorData.data();

    std::array<uint8_t, hdrSize + sizeof(struct query_device_identifiers_resp) +
                            descriptorDataLen>
        responseMsg{};
    struct query_device_identifiers_resp* inResp =
        reinterpret_cast<struct query_device_identifiers_resp*>(
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
              std::equal(descriptorData.begin(), descriptorData.end() - 1,
                         responseMsg.data() + hdrSize +
                             sizeof(struct query_device_identifiers_resp)));
}
TEST(GetFirmwareParameters, testGoodEncodeRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr)> requestMsg{};
    auto requestPtr = reinterpret_cast<pldm_msg*>(requestMsg.data());
    uint8_t instanceId = 0x01;

    auto rc = encode_get_firmware_parameters_req(
        instanceId, requestPtr, PLDM_GET_FIRMWARE_PARAMENTERS_REQ_BYTES);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(requestPtr->hdr.request, PLDM_REQUEST);
    EXPECT_EQ(requestPtr->hdr.instance_id, instanceId);
    EXPECT_EQ(requestPtr->hdr.type, PLDM_FWU);
    EXPECT_EQ(requestPtr->hdr.command, PLDM_GET_FIRMWARE_PARAMENTERS);
}
TEST(GetFWParams, testGoodDecodeCompImgSetResponse)
{
    // ActiveCompImageSetVerStrLen is not fixed here taking it as 8
    constexpr uint8_t activeCompImageSetVerStrLen = 8;
    // PendingCompImageSetVerStrLen is not fixed here taking it as 8
    constexpr uint8_t pendingCompImageSetVerStrLen = 8;

    constexpr size_t payloadLen = sizeof(struct get_firmware_parameters_resp) +
                                  activeCompImageSetVerStrLen +
                                  pendingCompImageSetVerStrLen;

    std::array<uint8_t, hdrSize + payloadLen> response{};
    struct get_firmware_parameters_resp* inResp =
        reinterpret_cast<struct get_firmware_parameters_resp*>(response.data() +
                                                               hdrSize);
    inResp->completion_code = PLDM_SUCCESS;
    inResp->capabilities_during_update = 0x0F;
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
        responseMsg, payloadLen, &outResp, &outActiveCompImageSetVerStr,
        &outPendingCompImageSetVerStr);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(inResp->completion_code, PLDM_SUCCESS);

    EXPECT_EQ(inResp->capabilities_during_update,
              outResp.capabilities_during_update);
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
    inCompData->active_comp_release_date = 0xFF,

    inCompData->pending_comp_comparison_stamp = 0;
    inCompData->pending_comp_ver_str_type = 1;
    inCompData->pending_comp_ver_str_len = pendingCompVerStrLen;
    inCompData->pending_comp_release_date = 0xFF;

    inCompData->comp_activation_methods = 0x0F;
    inCompData->capabilities_during_update = 0x0F;

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
    EXPECT_EQ(inCompData->active_comp_release_date,
              outCompData.active_comp_release_date);

    EXPECT_EQ(inCompData->pending_comp_comparison_stamp,
              outCompData.pending_comp_comparison_stamp);
    EXPECT_EQ(inCompData->pending_comp_ver_str_type,
              outCompData.pending_comp_ver_str_type);
    EXPECT_EQ(inCompData->pending_comp_ver_str_len,
              outCompData.pending_comp_ver_str_len);
    EXPECT_EQ(inCompData->pending_comp_release_date,
              outCompData.pending_comp_release_date);

    EXPECT_EQ(inCompData->comp_activation_methods,
              outCompData.comp_activation_methods);
    EXPECT_EQ(inCompData->capabilities_during_update,
              outCompData.capabilities_during_update);

    EXPECT_EQ(0, memcmp(outActiveCompVerStr.ptr,
                        response.data() + activeCompVerStrIndex,
                        outActiveCompVerStr.length));

    EXPECT_EQ(0, memcmp(outPendingCompVerStr.ptr,
                        response.data() + pendingCompVerStrIndex,
                        outPendingCompVerStr.length));
}