#include <bitset>
#include <cstring>

#include "libpldm/base.h"
#include "libpldm/firmware_update.h"

#include <gtest/gtest.h>

constexpr auto hdrSize = sizeof(pldm_msg_hdr);

TEST(DecodePackageHeaderInfo, goodPath)
{
    // Package header identifier for Version 1.0.x
    constexpr std::array<uint8_t, PLDM_FWUP_UUID_LENGTH> uuid{
        0xf0, 0x18, 0x87, 0x8c, 0xcb, 0x7d, 0x49, 0x43,
        0x98, 0x00, 0xa0, 0x2F, 0x05, 0x9a, 0xca, 0x02};
    // Package header version for DSP0267 version 1.0.x
    constexpr uint8_t pkgHeaderFormatRevision = 0x01;
    // Random PackageHeaderSize
    constexpr uint16_t pkgHeaderSize = 303;
    // PackageReleaseDateTime - "25/12/2021 00:00:00"
    std::array<uint8_t, PLDM_TIMESTAMP104_SIZE> timestamp104{
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x19, 0x0c, 0xe5, 0x07, 0x00};
    constexpr uint16_t componentBitmapBitLength = 8;
    // PackageVersionString
    constexpr std::string_view packageVersionStr{"OpenBMCv1.0"};
    constexpr size_t packagerHeaderSize =
        sizeof(pldm_package_header_information) + packageVersionStr.size();

    constexpr std::array<uint8_t, packagerHeaderSize> packagerHeaderInfo{
        0xf0, 0x18, 0x87, 0x8c, 0xcb, 0x7d, 0x49, 0x43, 0x98, 0x00, 0xa0, 0x2F,
        0x05, 0x9a, 0xca, 0x02, 0x01, 0x2f, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x19, 0x0c, 0xe5, 0x07, 0x00, 0x08, 0x00, 0x01, 0x0b,
        0x4f, 0x70, 0x65, 0x6e, 0x42, 0x4d, 0x43, 0x76, 0x31, 0x2e, 0x30};
    pldm_package_header_information pkgHeader{};
    variable_field packageVersion{};

    auto rc = decode_pldm_package_header_info(packagerHeaderInfo.data(),
                                              packagerHeaderInfo.size(),
                                              &pkgHeader, &packageVersion);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(true,
              std::equal(pkgHeader.uuid, pkgHeader.uuid + PLDM_FWUP_UUID_LENGTH,
                         uuid.begin(), uuid.end()));
    EXPECT_EQ(pkgHeader.package_header_format_version, pkgHeaderFormatRevision);
    EXPECT_EQ(pkgHeader.package_header_size, pkgHeaderSize);
    EXPECT_EQ(true, std::equal(pkgHeader.timestamp104,
                               pkgHeader.timestamp104 + PLDM_TIMESTAMP104_SIZE,
                               timestamp104.begin(), timestamp104.end()));
    EXPECT_EQ(pkgHeader.component_bitmap_bit_length, componentBitmapBitLength);
    EXPECT_EQ(pkgHeader.package_version_string_type, PLDM_STR_TYPE_ASCII);
    EXPECT_EQ(pkgHeader.package_version_string_length,
              packageVersionStr.size());
    std::string packageVersionString(
        reinterpret_cast<const char*>(packageVersion.ptr),
        packageVersion.length);
    EXPECT_EQ(packageVersionString, packageVersionStr);
}

TEST(DecodePackageHeaderInfo, errorPaths)
{
    int rc = 0;
    constexpr std::string_view packageVersionStr{"OpenBMCv1.0"};
    constexpr size_t packagerHeaderSize =
        sizeof(pldm_package_header_information) + packageVersionStr.size();

    // Invalid Package Version String Type - 0x06
    constexpr std::array<uint8_t, packagerHeaderSize>
        invalidPackagerHeaderInfo1{
            0xf0, 0x18, 0x87, 0x8c, 0xcb, 0x7d, 0x49, 0x43, 0x98, 0x00,
            0xa0, 0x2F, 0x05, 0x9a, 0xca, 0x02, 0x02, 0x2f, 0x01, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x0c, 0xe5,
            0x07, 0x00, 0x08, 0x00, 0x06, 0x0b, 0x4f, 0x70, 0x65, 0x6e,
            0x42, 0x4d, 0x43, 0x76, 0x31, 0x2e, 0x30};

    pldm_package_header_information packageHeader{};
    variable_field packageVersion{};

    rc = decode_pldm_package_header_info(nullptr,
                                         invalidPackagerHeaderInfo1.size(),
                                         &packageHeader, &packageVersion);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_pldm_package_header_info(invalidPackagerHeaderInfo1.data(),
                                         invalidPackagerHeaderInfo1.size(),
                                         nullptr, &packageVersion);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_pldm_package_header_info(invalidPackagerHeaderInfo1.data(),
                                         invalidPackagerHeaderInfo1.size(),
                                         &packageHeader, nullptr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_pldm_package_header_info(
        invalidPackagerHeaderInfo1.data(),
        sizeof(pldm_package_header_information) - 1, &packageHeader,
        &packageVersion);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    rc = decode_pldm_package_header_info(invalidPackagerHeaderInfo1.data(),
                                         invalidPackagerHeaderInfo1.size(),
                                         &packageHeader, &packageVersion);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Invalid Package Version String Length - 0x00
    constexpr std::array<uint8_t, packagerHeaderSize>
        invalidPackagerHeaderInfo2{
            0xf0, 0x18, 0x87, 0x8c, 0xcb, 0x7d, 0x49, 0x43, 0x98, 0x00,
            0xa0, 0x2F, 0x05, 0x9a, 0xca, 0x02, 0x02, 0x2f, 0x01, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x0c, 0xe5,
            0x07, 0x00, 0x08, 0x00, 0x01, 0x00, 0x4f, 0x70, 0x65, 0x6e,
            0x42, 0x4d, 0x43, 0x76, 0x31, 0x2e, 0x30};
    rc = decode_pldm_package_header_info(invalidPackagerHeaderInfo2.data(),
                                         invalidPackagerHeaderInfo2.size(),
                                         &packageHeader, &packageVersion);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Package version string length less than in the header information
    constexpr std::array<uint8_t, packagerHeaderSize - 1>
        invalidPackagerHeaderInfo3{
            0xf0, 0x18, 0x87, 0x8c, 0xcb, 0x7d, 0x49, 0x43, 0x98, 0x00,
            0xa0, 0x2F, 0x05, 0x9a, 0xca, 0x02, 0x02, 0x2f, 0x01, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x0c, 0xe5,
            0x07, 0x00, 0x08, 0x00, 0x01, 0x0b, 0x4f, 0x70, 0x65, 0x6e,
            0x42, 0x4d, 0x43, 0x76, 0x31, 0x2e};
    rc = decode_pldm_package_header_info(invalidPackagerHeaderInfo3.data(),
                                         invalidPackagerHeaderInfo3.size(),
                                         &packageHeader, &packageVersion);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    // ComponentBitmapBitLength not a multiple of 8
    constexpr std::array<uint8_t, packagerHeaderSize>
        invalidPackagerHeaderInfo4{
            0xf0, 0x18, 0x87, 0x8c, 0xcb, 0x7d, 0x49, 0x43, 0x98, 0x00,
            0xa0, 0x2F, 0x05, 0x9a, 0xca, 0x02, 0x02, 0x2f, 0x01, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x0c, 0xe5,
            0x07, 0x00, 0x09, 0x00, 0x01, 0x0b, 0x4f, 0x70, 0x65, 0x6e,
            0x42, 0x4d, 0x43, 0x76, 0x31, 0x2e, 0x30};
    rc = decode_pldm_package_header_info(invalidPackagerHeaderInfo4.data(),
                                         invalidPackagerHeaderInfo4.size(),
                                         &packageHeader, &packageVersion);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(DecodeFirmwareDeviceIdRecord, goodPath)
{
    constexpr uint8_t descriptorCount = 1;
    // Continue component updates after failure
    constexpr std::bitset<32> deviceUpdateFlag{1};
    constexpr uint16_t componentBitmapBitLength = 16;
    // Applicable Components - 1,2,5,8,9
    std::vector<std::bitset<8>> applicableComponentsBitfield{0x93, 0x01};
    // ComponentImageSetVersionString
    constexpr std::string_view imageSetVersionStr{"VersionString1"};
    // Initial descriptor - UUID
    constexpr std::array<uint8_t, PLDM_FWUP_UUID_LENGTH> uuid{
        0x12, 0x44, 0xd2, 0x64, 0x8d, 0x7d, 0x47, 0x18,
        0xa0, 0x30, 0xfc, 0x8a, 0x56, 0x58, 0x7d, 0x5b};
    constexpr uint16_t fwDevicePkgDataLen = 2;
    // FirmwareDevicePackageData
    constexpr std::array<uint8_t, fwDevicePkgDataLen> fwDevicePkgData{0xab,
                                                                      0xcd};
    // Size of the firmware device ID record
    constexpr uint16_t recordLen =
        sizeof(pldm_firmware_device_id_record) +
        (componentBitmapBitLength / PLDM_FWUP_COMPONENT_BITMAP_MULTIPLE) +
        imageSetVersionStr.size() + sizeof(pldm_descriptor_tlv) - 1 +
        uuid.size() + fwDevicePkgData.size();
    // Firmware device ID record
    constexpr std::array<uint8_t, recordLen> record{
        0x31, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0e, 0x02,
        0x00, 0x93, 0x01, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e,
        0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x31, 0x02, 0x00, 0x10,
        0x00, 0x12, 0x44, 0xd2, 0x64, 0x8d, 0x7d, 0x47, 0x18, 0xa0,
        0x30, 0xfc, 0x8a, 0x56, 0x58, 0x7d, 0x5b, 0xab, 0xcd};

    pldm_firmware_device_id_record deviceIdRecHeader{};
    variable_field applicableComponents{};
    variable_field outCompImageSetVersionStr{};
    variable_field recordDescriptors{};
    variable_field outFwDevicePkgData{};

    auto rc = decode_firmware_device_id_record(
        record.data(), record.size(), componentBitmapBitLength,
        &deviceIdRecHeader, &applicableComponents, &outCompImageSetVersionStr,
        &recordDescriptors, &outFwDevicePkgData);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(deviceIdRecHeader.record_length, recordLen);
    EXPECT_EQ(deviceIdRecHeader.descriptor_count, descriptorCount);
    EXPECT_EQ(deviceIdRecHeader.device_update_option_flags.value,
              deviceUpdateFlag);
    EXPECT_EQ(deviceIdRecHeader.comp_image_set_version_string_type,
              PLDM_STR_TYPE_ASCII);
    EXPECT_EQ(deviceIdRecHeader.comp_image_set_version_string_length,
              imageSetVersionStr.size());
    EXPECT_EQ(deviceIdRecHeader.fw_device_pkg_data_length, fwDevicePkgDataLen);

    EXPECT_EQ(applicableComponents.length, applicableComponentsBitfield.size());
    EXPECT_EQ(true,
              std::equal(applicableComponents.ptr,
                         applicableComponents.ptr + applicableComponents.length,
                         applicableComponentsBitfield.begin(),
                         applicableComponentsBitfield.end()));

    EXPECT_EQ(outCompImageSetVersionStr.length, imageSetVersionStr.size());
    std::string compImageSetVersionStr(
        reinterpret_cast<const char*>(outCompImageSetVersionStr.ptr),
        outCompImageSetVersionStr.length);
    EXPECT_EQ(compImageSetVersionStr, imageSetVersionStr);

    uint16_t descriptorType = 0;
    uint16_t descriptorLen = 0;
    variable_field descriptorData{};
    // DescriptorCount is 1, so decode_descriptor_type_length_value called once
    rc = decode_descriptor_type_length_value(recordDescriptors.ptr,
                                             recordDescriptors.length,
                                             &descriptorType, &descriptorData);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(recordDescriptors.length, sizeof(descriptorType) +
                                            sizeof(descriptorLen) +
                                            descriptorData.length);
    EXPECT_EQ(descriptorType, PLDM_FWUP_UUID);
    EXPECT_EQ(descriptorData.length, PLDM_FWUP_UUID_LENGTH);
    EXPECT_EQ(true, std::equal(descriptorData.ptr,
                               descriptorData.ptr + descriptorData.length,
                               uuid.begin(), uuid.end()));

    EXPECT_EQ(outFwDevicePkgData.length, fwDevicePkgData.size());
    EXPECT_EQ(true,
              std::equal(outFwDevicePkgData.ptr,
                         outFwDevicePkgData.ptr + outFwDevicePkgData.length,
                         fwDevicePkgData.begin(), fwDevicePkgData.end()));
}

TEST(DecodeFirmwareDeviceIdRecord, goodPathNofwDevicePkgData)
{
    constexpr uint8_t descriptorCount = 1;
    // Continue component updates after failure
    constexpr std::bitset<32> deviceUpdateFlag{1};
    constexpr uint16_t componentBitmapBitLength = 8;
    // Applicable Components - 1,2
    std::vector<std::bitset<8>> applicableComponentsBitfield{0x03};
    // ComponentImageSetVersionString
    constexpr std::string_view imageSetVersionStr{"VersionString1"};
    // Initial descriptor - UUID
    constexpr std::array<uint8_t, PLDM_FWUP_UUID_LENGTH> uuid{
        0x12, 0x44, 0xd2, 0x64, 0x8d, 0x7d, 0x47, 0x18,
        0xa0, 0x30, 0xfc, 0x8a, 0x56, 0x58, 0x7d, 0x5b};
    constexpr uint16_t fwDevicePkgDataLen = 0;

    // Size of the firmware device ID record
    constexpr uint16_t recordLen =
        sizeof(pldm_firmware_device_id_record) +
        (componentBitmapBitLength / PLDM_FWUP_COMPONENT_BITMAP_MULTIPLE) +
        imageSetVersionStr.size() +
        sizeof(pldm_descriptor_tlv().descriptor_type) +
        sizeof(pldm_descriptor_tlv().descriptor_length) + uuid.size() +
        fwDevicePkgDataLen;
    // Firmware device ID record
    constexpr std::array<uint8_t, recordLen> record{
        0x2e, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0e, 0x00, 0x00, 0x03,
        0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x53, 0x74, 0x72, 0x69, 0x6e,
        0x67, 0x31, 0x02, 0x00, 0x10, 0x00, 0x12, 0x44, 0xd2, 0x64, 0x8d, 0x7d,
        0x47, 0x18, 0xa0, 0x30, 0xfc, 0x8a, 0x56, 0x58, 0x7d, 0x5b};

    pldm_firmware_device_id_record deviceIdRecHeader{};
    variable_field applicableComponents{};
    variable_field outCompImageSetVersionStr{};
    variable_field recordDescriptors{};
    variable_field outFwDevicePkgData{};

    auto rc = decode_firmware_device_id_record(
        record.data(), record.size(), componentBitmapBitLength,
        &deviceIdRecHeader, &applicableComponents, &outCompImageSetVersionStr,
        &recordDescriptors, &outFwDevicePkgData);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(deviceIdRecHeader.record_length, recordLen);
    EXPECT_EQ(deviceIdRecHeader.descriptor_count, descriptorCount);
    EXPECT_EQ(deviceIdRecHeader.device_update_option_flags.value,
              deviceUpdateFlag);
    EXPECT_EQ(deviceIdRecHeader.comp_image_set_version_string_type,
              PLDM_STR_TYPE_ASCII);
    EXPECT_EQ(deviceIdRecHeader.comp_image_set_version_string_length,
              imageSetVersionStr.size());
    EXPECT_EQ(deviceIdRecHeader.fw_device_pkg_data_length, 0);

    EXPECT_EQ(applicableComponents.length, applicableComponentsBitfield.size());
    EXPECT_EQ(true,
              std::equal(applicableComponents.ptr,
                         applicableComponents.ptr + applicableComponents.length,
                         applicableComponentsBitfield.begin(),
                         applicableComponentsBitfield.end()));

    EXPECT_EQ(outCompImageSetVersionStr.length, imageSetVersionStr.size());
    std::string compImageSetVersionStr(
        reinterpret_cast<const char*>(outCompImageSetVersionStr.ptr),
        outCompImageSetVersionStr.length);
    EXPECT_EQ(compImageSetVersionStr, imageSetVersionStr);

    uint16_t descriptorType = 0;
    uint16_t descriptorLen = 0;
    variable_field descriptorData{};
    // DescriptorCount is 1, so decode_descriptor_type_length_value called once
    rc = decode_descriptor_type_length_value(recordDescriptors.ptr,
                                             recordDescriptors.length,
                                             &descriptorType, &descriptorData);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(recordDescriptors.length, sizeof(descriptorType) +
                                            sizeof(descriptorLen) +
                                            descriptorData.length);
    EXPECT_EQ(descriptorType, PLDM_FWUP_UUID);
    EXPECT_EQ(descriptorData.length, PLDM_FWUP_UUID_LENGTH);
    EXPECT_EQ(true, std::equal(descriptorData.ptr,
                               descriptorData.ptr + descriptorData.length,
                               uuid.begin(), uuid.end()));

    EXPECT_EQ(outFwDevicePkgData.ptr, nullptr);
    EXPECT_EQ(outFwDevicePkgData.length, 0);
}

TEST(DecodeFirmwareDeviceIdRecord, ErrorPaths)
{
    constexpr uint16_t componentBitmapBitLength = 8;
    // Invalid ComponentImageSetVersionStringType
    constexpr std::array<uint8_t, 11> invalidRecord1{
        0x0b, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x06, 0x0e, 0x00, 0x00};

    int rc = 0;
    pldm_firmware_device_id_record deviceIdRecHeader{};
    variable_field applicableComponents{};
    variable_field outCompImageSetVersionStr{};
    variable_field recordDescriptors{};
    variable_field outFwDevicePkgData{};

    rc = decode_firmware_device_id_record(
        nullptr, invalidRecord1.size(), componentBitmapBitLength,
        &deviceIdRecHeader, &applicableComponents, &outCompImageSetVersionStr,
        &recordDescriptors, &outFwDevicePkgData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_firmware_device_id_record(
        invalidRecord1.data(), invalidRecord1.size(), componentBitmapBitLength,
        nullptr, &applicableComponents, &outCompImageSetVersionStr,
        &recordDescriptors, &outFwDevicePkgData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_firmware_device_id_record(
        invalidRecord1.data(), invalidRecord1.size(), componentBitmapBitLength,
        &deviceIdRecHeader, nullptr, &outCompImageSetVersionStr,
        &recordDescriptors, &outFwDevicePkgData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_firmware_device_id_record(
        invalidRecord1.data(), invalidRecord1.size(), componentBitmapBitLength,
        &deviceIdRecHeader, &applicableComponents, nullptr, &recordDescriptors,
        &outFwDevicePkgData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_firmware_device_id_record(
        invalidRecord1.data(), invalidRecord1.size(), componentBitmapBitLength,
        &deviceIdRecHeader, &applicableComponents, &outCompImageSetVersionStr,
        nullptr, &outFwDevicePkgData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_firmware_device_id_record(
        invalidRecord1.data(), invalidRecord1.size(), componentBitmapBitLength,
        &deviceIdRecHeader, &applicableComponents, &outCompImageSetVersionStr,
        &recordDescriptors, nullptr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_firmware_device_id_record(
        invalidRecord1.data(), invalidRecord1.size() - 1,
        componentBitmapBitLength, &deviceIdRecHeader, &applicableComponents,
        &outCompImageSetVersionStr, &recordDescriptors, &outFwDevicePkgData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    rc = decode_firmware_device_id_record(
        invalidRecord1.data(), invalidRecord1.size(),
        componentBitmapBitLength + 1, &deviceIdRecHeader, &applicableComponents,
        &outCompImageSetVersionStr, &recordDescriptors, &outFwDevicePkgData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_firmware_device_id_record(
        invalidRecord1.data(), invalidRecord1.size(), componentBitmapBitLength,
        &deviceIdRecHeader, &applicableComponents, &outCompImageSetVersionStr,
        &recordDescriptors, &outFwDevicePkgData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Invalid ComponentImageSetVersionStringLength
    constexpr std::array<uint8_t, 11> invalidRecord2{
        0x0b, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
    rc = decode_firmware_device_id_record(
        invalidRecord2.data(), invalidRecord2.size(), componentBitmapBitLength,
        &deviceIdRecHeader, &applicableComponents, &outCompImageSetVersionStr,
        &recordDescriptors, &outFwDevicePkgData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // invalidRecord3 size is less than RecordLength
    constexpr std::array<uint8_t, 11> invalidRecord3{
        0x2e, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0e, 0x00, 0x00};
    rc = decode_firmware_device_id_record(
        invalidRecord3.data(), invalidRecord3.size(), componentBitmapBitLength,
        &deviceIdRecHeader, &applicableComponents, &outCompImageSetVersionStr,
        &recordDescriptors, &outFwDevicePkgData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    // RecordLength is less than the calculated RecordLength
    constexpr std::array<uint8_t, 11> invalidRecord4{
        0x15, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0e, 0x02, 0x00};
    rc = decode_firmware_device_id_record(
        invalidRecord4.data(), invalidRecord4.size(), componentBitmapBitLength,
        &deviceIdRecHeader, &applicableComponents, &outCompImageSetVersionStr,
        &recordDescriptors, &outFwDevicePkgData);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

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

TEST(DecodeComponentImageInfo, goodPath)
{
    // Firmware
    constexpr uint16_t compClassification = 16;
    constexpr uint16_t compIdentifier = 300;
    constexpr uint32_t compComparisonStamp = 0xFFFFFFFF;
    // Force update
    constexpr std::bitset<16> compOptions{1};
    // System reboot[Bit position 3] & Medium-specific reset[Bit position 2]
    constexpr std::bitset<16> reqCompActivationMethod{0x0c};
    // Random ComponentLocationOffset
    constexpr uint32_t compLocOffset = 357;
    // Random ComponentSize
    constexpr uint32_t compSize = 27;
    // ComponentVersionString
    constexpr std::string_view compVersionStr{"VersionString1"};
    constexpr size_t compImageInfoSize =
        sizeof(pldm_component_image_information) + compVersionStr.size();

    constexpr std::array<uint8_t, compImageInfoSize> compImageInfo{
        0x10, 0x00, 0x2c, 0x01, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x0c, 0x00,
        0x65, 0x01, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x01, 0x0e, 0x56, 0x65,
        0x72, 0x73, 0x69, 0x6f, 0x6e, 0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x31};
    pldm_component_image_information outCompImageInfo{};
    variable_field outCompVersionStr{};

    auto rc =
        decode_pldm_comp_image_info(compImageInfo.data(), compImageInfo.size(),
                                    &outCompImageInfo, &outCompVersionStr);

    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(outCompImageInfo.comp_classification, compClassification);
    EXPECT_EQ(outCompImageInfo.comp_identifier, compIdentifier);
    EXPECT_EQ(outCompImageInfo.comp_comparison_stamp, compComparisonStamp);
    EXPECT_EQ(outCompImageInfo.comp_options.value, compOptions);
    EXPECT_EQ(outCompImageInfo.requested_comp_activation_method.value,
              reqCompActivationMethod);
    EXPECT_EQ(outCompImageInfo.comp_location_offset, compLocOffset);
    EXPECT_EQ(outCompImageInfo.comp_size, compSize);
    EXPECT_EQ(outCompImageInfo.comp_version_string_type, PLDM_STR_TYPE_ASCII);
    EXPECT_EQ(outCompImageInfo.comp_version_string_length,
              compVersionStr.size());

    EXPECT_EQ(outCompVersionStr.length,
              outCompImageInfo.comp_version_string_length);
    std::string componentVersionString(
        reinterpret_cast<const char*>(outCompVersionStr.ptr),
        outCompVersionStr.length);
    EXPECT_EQ(componentVersionString, compVersionStr);
}

TEST(DecodeComponentImageInfo, errorPaths)
{
    int rc = 0;
    // ComponentVersionString
    constexpr std::string_view compVersionStr{"VersionString1"};
    constexpr size_t compImageInfoSize =
        sizeof(pldm_component_image_information) + compVersionStr.size();
    // Invalid ComponentVersionStringType - 0x06
    constexpr std::array<uint8_t, compImageInfoSize> invalidCompImageInfo1{
        0x10, 0x00, 0x2c, 0x01, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x0c, 0x00,
        0x65, 0x01, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x06, 0x0e, 0x56, 0x65,
        0x72, 0x73, 0x69, 0x6f, 0x6e, 0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x31};
    pldm_component_image_information outCompImageInfo{};
    variable_field outCompVersionStr{};

    rc = decode_pldm_comp_image_info(nullptr, invalidCompImageInfo1.size(),
                                     &outCompImageInfo, &outCompVersionStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_pldm_comp_image_info(invalidCompImageInfo1.data(),
                                     invalidCompImageInfo1.size(), nullptr,
                                     &outCompVersionStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_pldm_comp_image_info(invalidCompImageInfo1.data(),
                                     invalidCompImageInfo1.size(),
                                     &outCompImageInfo, nullptr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc = decode_pldm_comp_image_info(invalidCompImageInfo1.data(),
                                     sizeof(pldm_component_image_information) -
                                         1,
                                     &outCompImageInfo, &outCompVersionStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    rc = decode_pldm_comp_image_info(invalidCompImageInfo1.data(),
                                     invalidCompImageInfo1.size(),
                                     &outCompImageInfo, &outCompVersionStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Invalid ComponentVersionStringLength - 0x00
    constexpr std::array<uint8_t, compImageInfoSize> invalidCompImageInfo2{
        0x10, 0x00, 0x2c, 0x01, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x0c, 0x00,
        0x65, 0x01, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x56, 0x65,
        0x72, 0x73, 0x69, 0x6f, 0x6e, 0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x31};
    rc = decode_pldm_comp_image_info(invalidCompImageInfo2.data(),
                                     invalidCompImageInfo2.size(),
                                     &outCompImageInfo, &outCompVersionStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Use Component Comparison Stamp is not set, but ComponentComparisonStamp
    // is not 0xFFFFFFFF
    constexpr std::array<uint8_t, compImageInfoSize> invalidCompImageInfo3{
        0x10, 0x00, 0x2c, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x0c, 0x00,
        0x65, 0x01, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x01, 0x0e, 0x56, 0x65,
        0x72, 0x73, 0x69, 0x6f, 0x6e, 0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x31};

    rc = decode_pldm_comp_image_info(invalidCompImageInfo3.data(),
                                     invalidCompImageInfo3.size() - 1,
                                     &outCompImageInfo, &outCompVersionStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    rc = decode_pldm_comp_image_info(invalidCompImageInfo3.data(),
                                     invalidCompImageInfo3.size(),
                                     &outCompImageInfo, &outCompVersionStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Invalid ComponentLocationOffset - 0
    constexpr std::array<uint8_t, compImageInfoSize> invalidCompImageInfo4{
        0x10, 0x00, 0x2c, 0x01, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x0c, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x01, 0x0e, 0x56, 0x65,
        0x72, 0x73, 0x69, 0x6f, 0x6e, 0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x31};
    rc = decode_pldm_comp_image_info(invalidCompImageInfo4.data(),
                                     invalidCompImageInfo4.size(),
                                     &outCompImageInfo, &outCompVersionStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    // Invalid ComponentSize - 0
    constexpr std::array<uint8_t, compImageInfoSize> invalidCompImageInfo5{
        0x10, 0x00, 0x2c, 0x01, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x0c, 0x00,
        0x65, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x0e, 0x56, 0x65,
        0x72, 0x73, 0x69, 0x6f, 0x6e, 0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x31};
    rc = decode_pldm_comp_image_info(invalidCompImageInfo5.data(),
                                     invalidCompImageInfo5.size(),
                                     &outCompImageInfo, &outCompVersionStr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
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
