#include <bitset>
#include <cstring>

#include "libpldm/base.h"
#include "libpldm/firmware_update.h"

#include <gtest/gtest.h>

constexpr auto hdrSize = sizeof(pldm_msg_hdr);

TEST(DecodeDescriptors, goodPath3Descriptors)
{
    // In the descriptor data there are 3 descriptor entries
    // 1) IANA enterprise ID
    constexpr std::array<uint8_t, PLDM_FWUP_IANA_ENTERPRISE_ID_LENGTH> iana{
        0x0a, 0x0b, 0x0c, 0xd};
    // 2) UUID
    constexpr std::array<uint8_t, PLDM_FWUP_UUID_LENGTH> uuid{
        0x12, 0x44, 0xd2, 0x64, 0x8d, 0x7d, 0x47, 0x18,
        0xa0, 0x30, 0xfc, 0x8a, 0x56, 0x58, 0x7d, 0x5b};
    // 3) Vendor Defined
    constexpr std::string_view vendorTitle{"OpenBMC"};
    constexpr size_t vendorDescriptorLen = 2;
    constexpr std::array<uint8_t, vendorDescriptorLen> vendorDescriptorData{
        0x01, 0x02};

    constexpr size_t vendorDefinedDescriptorLen =
        sizeof(pldm_vendor_defined_descriptor_title_data()
                   .vendor_defined_descriptor_title_str_type) +
        sizeof(pldm_vendor_defined_descriptor_title_data()
                   .vendor_defined_descriptor_title_str_len) +
        vendorTitle.size() + vendorDescriptorData.size();

    constexpr size_t descriptorsLength =
        3 * (sizeof(pldm_descriptor_tlv().descriptor_type) +
             sizeof(pldm_descriptor_tlv().descriptor_length)) +
        iana.size() + uuid.size() + vendorDefinedDescriptorLen;

    constexpr std::array<uint8_t, descriptorsLength> descriptors{
        0x01, 0x00, 0x04, 0x00, 0x0a, 0x0b, 0x0c, 0x0d, 0x02, 0x00, 0x10,
        0x00, 0x12, 0x44, 0xd2, 0x64, 0x8d, 0x7d, 0x47, 0x18, 0xa0, 0x30,
        0xfc, 0x8a, 0x56, 0x58, 0x7d, 0x5b, 0xFF, 0xFF, 0x0B, 0x00, 0x01,
        0x07, 0x4f, 0x70, 0x65, 0x6e, 0x42, 0x4d, 0x43, 0x01, 0x02};

    size_t descriptorCount = 1;
    size_t descriptorsRemainingLength = descriptorsLength;
    int rc = 0;

    while (descriptorsRemainingLength && (descriptorCount <= 3))
    {
        uint16_t descriptorType = 0;
        uint16_t descriptorLen = 0;
        variable_field descriptorData{};

        rc = decode_descriptor_type_length_value(
            descriptors.data() + descriptorsLength - descriptorsRemainingLength,
            descriptorsRemainingLength, &descriptorType, &descriptorData);
        EXPECT_EQ(rc, PLDM_SUCCESS);

        if (descriptorCount == 1)
        {
            EXPECT_EQ(descriptorType, PLDM_FWUP_IANA_ENTERPRISE_ID);
            EXPECT_EQ(descriptorData.length,
                      PLDM_FWUP_IANA_ENTERPRISE_ID_LENGTH);
            EXPECT_EQ(true,
                      std::equal(descriptorData.ptr,
                                 descriptorData.ptr + descriptorData.length,
                                 iana.begin(), iana.end()));
        }
        else if (descriptorCount == 2)
        {
            EXPECT_EQ(descriptorType, PLDM_FWUP_UUID);
            EXPECT_EQ(descriptorData.length, PLDM_FWUP_UUID_LENGTH);
            EXPECT_EQ(true,
                      std::equal(descriptorData.ptr,
                                 descriptorData.ptr + descriptorData.length,
                                 uuid.begin(), uuid.end()));
        }
        else if (descriptorCount == 3)
        {
            EXPECT_EQ(descriptorType, PLDM_FWUP_VENDOR_DEFINED);
            EXPECT_EQ(descriptorData.length, vendorDefinedDescriptorLen);

            uint8_t descriptorTitleStrType = 0;
            variable_field descriptorTitleStr{};
            variable_field vendorDefinedDescriptorData{};

            rc = decode_vendor_defined_descriptor_value(
                descriptorData.ptr, descriptorData.length,
                &descriptorTitleStrType, &descriptorTitleStr,
                &vendorDefinedDescriptorData);
            EXPECT_EQ(rc, PLDM_SUCCESS);

            EXPECT_EQ(descriptorTitleStrType, PLDM_STR_TYPE_ASCII);
            EXPECT_EQ(descriptorTitleStr.length, vendorTitle.size());
            std::string vendorTitleStr(
                reinterpret_cast<const char*>(descriptorTitleStr.ptr),
                descriptorTitleStr.length);
            EXPECT_EQ(vendorTitleStr, vendorTitle);

            EXPECT_EQ(vendorDefinedDescriptorData.length,
                      vendorDescriptorData.size());
            EXPECT_EQ(true, std::equal(vendorDefinedDescriptorData.ptr,
                                       vendorDefinedDescriptorData.ptr +
                                           vendorDefinedDescriptorData.length,
                                       vendorDescriptorData.begin(),
                                       vendorDescriptorData.end()));
        }

        descriptorsRemainingLength -= sizeof(descriptorType) +
                                      sizeof(descriptorLen) +
                                      descriptorData.length;
        descriptorCount++;
    }
}

TEST(DecodeDescriptors, errorPathDecodeDescriptorTLV)
{
    int rc = 0;
    // IANA Enterprise ID descriptor length incorrect
    constexpr std::array<uint8_t, 7> invalidIANADescriptor1{
        0x01, 0x00, 0x03, 0x00, 0x0a, 0x0b, 0x0c};
    uint16_t descriptorType = 0;
    variable_field descriptorData{};

    rc = decode_descriptor_type_length_value(nullptr,
                                             invalidIANADescriptor1.size(),
                                             &descriptorType, &descriptorData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_descriptor_type_length_value(invalidIANADescriptor1.data(),
                                             invalidIANADescriptor1.size(),
                                             nullptr, &descriptorData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_descriptor_type_length_value(invalidIANADescriptor1.data(),
                                             invalidIANADescriptor1.size(),
                                             &descriptorType, nullptr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_descriptor_type_length_value(
        invalidIANADescriptor1.data(), PLDM_FWUP_DEVICE_DESCRIPTOR_MIN_LEN - 1,
        &descriptorType, &descriptorData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    rc = decode_descriptor_type_length_value(invalidIANADescriptor1.data(),
                                             invalidIANADescriptor1.size(),
                                             &descriptorType, &descriptorData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    // IANA Enterprise ID descriptor data less than length
    std::array<uint8_t, 7> invalidIANADescriptor2{0x01, 0x00, 0x04, 0x00,
                                                  0x0a, 0x0b, 0x0c};
    rc = decode_descriptor_type_length_value(invalidIANADescriptor2.data(),
                                             invalidIANADescriptor2.size(),
                                             &descriptorType, &descriptorData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(DecodeDescriptors, errorPathVendorDefinedDescriptor)
{
    int rc = 0;
    // VendorDefinedDescriptorTitleStringType is invalid
    constexpr std::array<uint8_t, 9> invalidVendorDescriptor1{
        0x06, 0x07, 0x4f, 0x70, 0x65, 0x6e, 0x42, 0x4d, 0x43};
    uint8_t descriptorStringType = 0;
    variable_field descriptorTitleStr{};
    variable_field vendorDefinedDescriptorData{};

    rc = decode_vendor_defined_descriptor_value(
        nullptr, invalidVendorDescriptor1.size(), &descriptorStringType,
        &descriptorTitleStr, &vendorDefinedDescriptorData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_vendor_defined_descriptor_value(
        invalidVendorDescriptor1.data(), invalidVendorDescriptor1.size(),
        &descriptorStringType, &descriptorTitleStr,
        &vendorDefinedDescriptorData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_vendor_defined_descriptor_value(
        invalidVendorDescriptor1.data(), invalidVendorDescriptor1.size(),
        nullptr, &descriptorTitleStr, &vendorDefinedDescriptorData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_vendor_defined_descriptor_value(
        invalidVendorDescriptor1.data(), invalidVendorDescriptor1.size(),
        &descriptorStringType, nullptr, &vendorDefinedDescriptorData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_vendor_defined_descriptor_value(
        invalidVendorDescriptor1.data(), invalidVendorDescriptor1.size(),
        &descriptorStringType, &descriptorTitleStr, nullptr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_vendor_defined_descriptor_value(
        invalidVendorDescriptor1.data(),
        sizeof(pldm_vendor_defined_descriptor_title_data) - 1,
        &descriptorStringType, &descriptorTitleStr,
        &vendorDefinedDescriptorData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    rc = decode_vendor_defined_descriptor_value(
        invalidVendorDescriptor1.data(), invalidVendorDescriptor1.size(),
        &descriptorStringType, &descriptorTitleStr,
        &vendorDefinedDescriptorData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // VendorDefinedDescriptorTitleStringLength is 0
    std::array<uint8_t, 9> invalidVendorDescriptor2{
        0x01, 0x00, 0x4f, 0x70, 0x65, 0x6e, 0x42, 0x4d, 0x43};
    rc = decode_vendor_defined_descriptor_value(
        invalidVendorDescriptor2.data(), invalidVendorDescriptor2.size(),
        &descriptorStringType, &descriptorTitleStr,
        &vendorDefinedDescriptorData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // VendorDefinedDescriptorData not present in the data
    std::array<uint8_t, 9> invalidVendorDescriptor3{
        0x01, 0x07, 0x4f, 0x70, 0x65, 0x6e, 0x42, 0x4d, 0x43};
    rc = decode_vendor_defined_descriptor_value(
        invalidVendorDescriptor3.data(), invalidVendorDescriptor3.size(),
        &descriptorStringType, &descriptorTitleStr,
        &vendorDefinedDescriptorData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

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

TEST(GetFirmwareParameters, decodeResponse)
{
    // CapabilitiesDuringUpdate of the firmware device
    // Firmware device downgrade restrictions [Bit position 8] &
    // Firmware Device Partial Updates [Bit position 3]
    constexpr std::bitset<32> fdCapabilities{0x00000104};
    constexpr uint16_t compCount = 1;
    constexpr std::string_view activeCompImageSetVersion{"VersionString1"};
    constexpr std::string_view pendingCompImageSetVersion{"VersionString2"};

    // constexpr uint16_t compClassification = 16;
    // constexpr uint16_t compIdentifier = 300;
    // constexpr uint8_t compClassificationIndex = 20;
    // constexpr uint32_t activeCompComparisonStamp = 0xABCDEFAB;
    // constexpr std::array<uint8_t, 8> activeComponentReleaseData = {
    //     0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    // constexpr uint32_t pendingCompComparisonStamp = 0x12345678;
    // constexpr std::array<uint8_t, 8> pendingComponentReleaseData = {
    //     0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
    constexpr std::string_view activeCompVersion{"VersionString3"};
    constexpr std::string_view pendingCompVersion{"VersionString4"};
    // ComponentActivationMethods
    // DC Power cycle [Bit position 4] & Self-Contained[Bit position 2]
    constexpr std::bitset<16> compActivationMethod{0x12};
    // CapabilitiesDuringUpdate of the firmware component
    // Component downgrade capability [Bit position 2]
    constexpr std::bitset<32> compCapabilities{0x02};

    constexpr size_t compParamTableSize =
        sizeof(pldm_component_parameter_entry) + activeCompVersion.size() +
        pendingCompVersion.size();

    constexpr std::array<uint8_t, compParamTableSize> compParamTable{
        0x10, 0x00, 0x2c, 0x01, 0x14, 0xAB, 0xEF, 0xCD, 0xAB, 0x01, 0x0e, 0x01,
        0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x78, 0x56, 0x34, 0x12, 0x01,
        0x0e, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x12, 0x00, 0x02,
        0x00, 0x00, 0x00, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x53, 0x74,
        0x72, 0x69, 0x6e, 0x67, 0x33, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e,
        0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x34};

    constexpr size_t getFwParamsPayloadLen =
        sizeof(pldm_get_firmware_parameters_resp) +
        activeCompImageSetVersion.size() + pendingCompImageSetVersion.size() +
        compParamTableSize;

    constexpr std::array<uint8_t, hdrSize + getFwParamsPayloadLen>
        getFwParamsResponse{
            0x00, 0x00, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01,
            0x0e, 0x01, 0x0e, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x53,
            0x74, 0x72, 0x69, 0x6e, 0x67, 0x31, 0x56, 0x65, 0x72, 0x73, 0x69,
            0x6f, 0x6e, 0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x32, 0x10, 0x00,
            0x2c, 0x01, 0x14, 0xAB, 0xEF, 0xCD, 0xAB, 0x01, 0x0e, 0x01, 0x02,
            0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x78, 0x56, 0x34, 0x12, 0x01,
            0x0e, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x12, 0x00,
            0x02, 0x00, 0x00, 0x00, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e,
            0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x33, 0x56, 0x65, 0x72, 0x73,
            0x69, 0x6f, 0x6e, 0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x34};

    auto responseMsg =
        reinterpret_cast<const pldm_msg*>(getFwParamsResponse.data());
    pldm_get_firmware_parameters_resp outResp{};
    variable_field outActiveCompImageSetVersion{};
    variable_field outPendingCompImageSetVersion{};
    variable_field outCompParameterTable{};

    auto rc = decode_get_firmware_parameters_resp(
        responseMsg, getFwParamsPayloadLen, &outResp,
        &outActiveCompImageSetVersion, &outPendingCompImageSetVersion,
        &outCompParameterTable);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(outResp.completion_code, PLDM_SUCCESS);
    EXPECT_EQ(outResp.capabilities_during_update.value, fdCapabilities);
    EXPECT_EQ(outResp.comp_count, compCount);
    EXPECT_EQ(outResp.active_comp_image_set_ver_str_type, PLDM_STR_TYPE_ASCII);
    EXPECT_EQ(outResp.active_comp_image_set_ver_str_len,
              activeCompImageSetVersion.size());
    EXPECT_EQ(outResp.pending_comp_image_set_ver_str_type, PLDM_STR_TYPE_ASCII);
    EXPECT_EQ(outResp.pending_comp_image_set_ver_str_len,
              pendingCompImageSetVersion.size());
    std::string activeCompImageSetVersionStr(
        reinterpret_cast<const char*>(outActiveCompImageSetVersion.ptr),
        outActiveCompImageSetVersion.length);
    EXPECT_EQ(activeCompImageSetVersionStr, activeCompImageSetVersion);
    std::string pendingCompImageSetVersionStr(
        reinterpret_cast<const char*>(outPendingCompImageSetVersion.ptr),
        outPendingCompImageSetVersion.length);
    EXPECT_EQ(pendingCompImageSetVersionStr, pendingCompImageSetVersion);
    EXPECT_EQ(outCompParameterTable.length, compParamTableSize);
    EXPECT_EQ(true, std::equal(outCompParameterTable.ptr,
                               outCompParameterTable.ptr +
                                   outCompParameterTable.length,
                               compParamTable.begin(), compParamTable.end()));
}

TEST(GetFirmwareParameters, decodeResponseZeroCompCount)
{
    // CapabilitiesDuringUpdate of the firmware device
    // FD Host Functionality during Firmware Update [Bit position 2] &
    // Component Update Failure Retry Capability [Bit position 1]
    constexpr std::bitset<32> fdCapabilities{0x06};
    constexpr uint16_t compCount = 0;
    constexpr std::string_view activeCompImageSetVersion{"VersionString1"};
    constexpr std::string_view pendingCompImageSetVersion{"VersionString2"};

    constexpr size_t getFwParamsPayloadLen =
        sizeof(pldm_get_firmware_parameters_resp) +
        activeCompImageSetVersion.size() + pendingCompImageSetVersion.size();

    constexpr std::array<uint8_t, hdrSize + getFwParamsPayloadLen>
        getFwParamsResponse{
            0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
            0x0e, 0x01, 0x0e, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x53,
            0x74, 0x72, 0x69, 0x6e, 0x67, 0x31, 0x56, 0x65, 0x72, 0x73, 0x69,
            0x6f, 0x6e, 0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x32};

    auto responseMsg =
        reinterpret_cast<const pldm_msg*>(getFwParamsResponse.data());
    pldm_get_firmware_parameters_resp outResp{};
    variable_field outActiveCompImageSetVersion{};
    variable_field outPendingCompImageSetVersion{};
    variable_field outCompParameterTable{};

    auto rc = decode_get_firmware_parameters_resp(
        responseMsg, getFwParamsPayloadLen, &outResp,
        &outActiveCompImageSetVersion, &outPendingCompImageSetVersion,
        &outCompParameterTable);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(outResp.completion_code, PLDM_SUCCESS);
    EXPECT_EQ(outResp.capabilities_during_update.value, fdCapabilities);
    EXPECT_EQ(outResp.comp_count, compCount);
    EXPECT_EQ(outResp.active_comp_image_set_ver_str_type, PLDM_STR_TYPE_ASCII);
    EXPECT_EQ(outResp.active_comp_image_set_ver_str_len,
              activeCompImageSetVersion.size());
    EXPECT_EQ(outResp.pending_comp_image_set_ver_str_type, PLDM_STR_TYPE_ASCII);
    EXPECT_EQ(outResp.pending_comp_image_set_ver_str_len,
              pendingCompImageSetVersion.size());
    std::string activeCompImageSetVersionStr(
        reinterpret_cast<const char*>(outActiveCompImageSetVersion.ptr),
        outActiveCompImageSetVersion.length);
    EXPECT_EQ(activeCompImageSetVersionStr, activeCompImageSetVersion);
    std::string pendingCompImageSetVersionStr(
        reinterpret_cast<const char*>(outPendingCompImageSetVersion.ptr),
        outPendingCompImageSetVersion.length);
    EXPECT_EQ(pendingCompImageSetVersionStr, pendingCompImageSetVersion);
    EXPECT_EQ(outCompParameterTable.ptr, nullptr);
    EXPECT_EQ(outCompParameterTable.length, 0);
}

TEST(GetFirmwareParameters,
     decodeResponseNoPendingCompImageVersionStrZeroCompCount)
{
    // CapabilitiesDuringUpdate of the firmware device
    // FD Host Functionality during Firmware Update [Bit position 2] &
    // Component Update Failure Retry Capability [Bit position 1]
    constexpr std::bitset<32> fdCapabilities{0x06};
    constexpr uint16_t compCount = 0;
    constexpr std::string_view activeCompImageSetVersion{"VersionString"};

    constexpr size_t getFwParamsPayloadLen =
        sizeof(pldm_get_firmware_parameters_resp) +
        activeCompImageSetVersion.size();

    constexpr std::array<uint8_t, hdrSize + getFwParamsPayloadLen>
        getFwParamsResponse{0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x01, 0x0d, 0x00, 0x00,
                            0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e,
                            0x53, 0x74, 0x72, 0x69, 0x6e, 0x67};

    auto responseMsg =
        reinterpret_cast<const pldm_msg*>(getFwParamsResponse.data());
    pldm_get_firmware_parameters_resp outResp{};
    variable_field outActiveCompImageSetVersion{};
    variable_field outPendingCompImageSetVersion{};
    variable_field outCompParameterTable{};

    auto rc = decode_get_firmware_parameters_resp(
        responseMsg, getFwParamsPayloadLen, &outResp,
        &outActiveCompImageSetVersion, &outPendingCompImageSetVersion,
        &outCompParameterTable);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(outResp.completion_code, PLDM_SUCCESS);
    EXPECT_EQ(outResp.capabilities_during_update.value, fdCapabilities);
    EXPECT_EQ(outResp.comp_count, compCount);
    EXPECT_EQ(outResp.active_comp_image_set_ver_str_type, PLDM_STR_TYPE_ASCII);
    EXPECT_EQ(outResp.active_comp_image_set_ver_str_len,
              activeCompImageSetVersion.size());
    EXPECT_EQ(outResp.pending_comp_image_set_ver_str_type,
              PLDM_STR_TYPE_UNKNOWN);
    EXPECT_EQ(outResp.pending_comp_image_set_ver_str_len, 0);
    std::string activeCompImageSetVersionStr(
        reinterpret_cast<const char*>(outActiveCompImageSetVersion.ptr),
        outActiveCompImageSetVersion.length);
    EXPECT_EQ(activeCompImageSetVersionStr, activeCompImageSetVersion);
    EXPECT_EQ(outPendingCompImageSetVersion.ptr, nullptr);
    EXPECT_EQ(outPendingCompImageSetVersion.length, 0);
    EXPECT_EQ(outCompParameterTable.ptr, nullptr);
    EXPECT_EQ(outCompParameterTable.length, 0);
}

TEST(GetFirmwareParameters, decodeResponseErrorCompletionCode)
{
    constexpr std::array<uint8_t,
                         hdrSize + sizeof(pldm_get_firmware_parameters_resp)>
        getFwParamsResponse{0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    auto responseMsg =
        reinterpret_cast<const pldm_msg*>(getFwParamsResponse.data());
    pldm_get_firmware_parameters_resp outResp{};
    variable_field outActiveCompImageSetVersion{};
    variable_field outPendingCompImageSetVersion{};
    variable_field outCompParameterTable{};

    auto rc = decode_get_firmware_parameters_resp(
        responseMsg, getFwParamsResponse.size(), &outResp,
        &outActiveCompImageSetVersion, &outPendingCompImageSetVersion,
        &outCompParameterTable);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(outResp.completion_code, PLDM_ERROR);
}

TEST(GetFirmwareParameters, errorPathdecodeResponse)
{
    int rc = 0;
    // Invalid ActiveComponentImageSetVersionStringType
    constexpr std::array<uint8_t, 14> invalidGetFwParamsResponse1{
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x06, 0x0e, 0x00, 0x00};

    auto responseMsg =
        reinterpret_cast<const pldm_msg*>(invalidGetFwParamsResponse1.data());
    pldm_get_firmware_parameters_resp outResp{};
    variable_field outActiveCompImageSetVersion{};
    variable_field outPendingCompImageSetVersion{};
    variable_field outCompParameterTable{};

    rc = decode_get_firmware_parameters_resp(
        nullptr, invalidGetFwParamsResponse1.size(), &outResp,
        &outActiveCompImageSetVersion, &outPendingCompImageSetVersion,
        &outCompParameterTable);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_firmware_parameters_resp(
        responseMsg, invalidGetFwParamsResponse1.size(), nullptr,
        &outActiveCompImageSetVersion, &outPendingCompImageSetVersion,
        &outCompParameterTable);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_firmware_parameters_resp(
        responseMsg, invalidGetFwParamsResponse1.size(), &outResp, nullptr,
        &outPendingCompImageSetVersion, &outCompParameterTable);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_firmware_parameters_resp(
        responseMsg, invalidGetFwParamsResponse1.size(), &outResp,
        &outActiveCompImageSetVersion, nullptr, &outCompParameterTable);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_firmware_parameters_resp(
        responseMsg, invalidGetFwParamsResponse1.size(), &outResp,
        &outActiveCompImageSetVersion, &outPendingCompImageSetVersion, nullptr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_get_firmware_parameters_resp(
        responseMsg, 0, &outResp, &outActiveCompImageSetVersion,
        &outPendingCompImageSetVersion, &outCompParameterTable);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    rc = decode_get_firmware_parameters_resp(
        responseMsg, invalidGetFwParamsResponse1.size(), &outResp,
        &outActiveCompImageSetVersion, &outPendingCompImageSetVersion,
        &outCompParameterTable);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Invalid ActiveComponentImageSetVersionStringLength
    constexpr std::array<uint8_t, 14> invalidGetFwParamsResponse2{
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
    responseMsg =
        reinterpret_cast<const pldm_msg*>(invalidGetFwParamsResponse2.data());
    rc = decode_get_firmware_parameters_resp(
        responseMsg, invalidGetFwParamsResponse2.size(), &outResp,
        &outActiveCompImageSetVersion, &outPendingCompImageSetVersion,
        &outCompParameterTable);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Invalid PendingComponentImageSetVersionStringType &
    // PendingComponentImageSetVersionStringLength
    constexpr std::array<uint8_t, 14> invalidGetFwParamsResponse3{
        0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x0e, 0x01, 0x00};
    responseMsg =
        reinterpret_cast<const pldm_msg*>(invalidGetFwParamsResponse3.data());
    rc = decode_get_firmware_parameters_resp(
        responseMsg, invalidGetFwParamsResponse3.size(), &outResp,
        &outActiveCompImageSetVersion, &outPendingCompImageSetVersion,
        &outCompParameterTable);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Invalid PendingComponentImageSetVersionStringType &
    // PendingComponentImageSetVersionStringLength
    constexpr std::array<uint8_t, 14> invalidGetFwParamsResponse4{
        0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x0e, 0x06, 0x0e};
    responseMsg =
        reinterpret_cast<const pldm_msg*>(invalidGetFwParamsResponse4.data());
    rc = decode_get_firmware_parameters_resp(
        responseMsg, invalidGetFwParamsResponse4.size(), &outResp,
        &outActiveCompImageSetVersion, &outPendingCompImageSetVersion,
        &outCompParameterTable);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Total payload length less than expected
    constexpr std::array<uint8_t, 14> invalidGetFwParamsResponse5{
        0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x0e, 0x01, 0x0e};
    responseMsg =
        reinterpret_cast<const pldm_msg*>(invalidGetFwParamsResponse5.data());
    rc = decode_get_firmware_parameters_resp(
        responseMsg, invalidGetFwParamsResponse5.size(), &outResp,
        &outActiveCompImageSetVersion, &outPendingCompImageSetVersion,
        &outCompParameterTable);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
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
