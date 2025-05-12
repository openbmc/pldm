#include "package_parser.hpp"

#include "common/utils.hpp"

#include <libpldm/firmware_update.h>
#include <libpldm/utils.h>

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <memory>

PHOSPHOR_LOG2_USING;

namespace pldm
{

namespace fw_update
{

using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

size_t PackageParser::parseFDIdentificationArea(
    DeviceIDRecordCount deviceIdRecCount, const std::vector<uint8_t>& pkgHdr,
    size_t offset)
{
    size_t pkgHdrRemainingSize = pkgHdr.size() - offset;

    while (deviceIdRecCount-- && (pkgHdrRemainingSize > 0))
    {
        pldm_firmware_device_id_record deviceIdRecHeader{};
        variable_field applicableComponents{};
        variable_field compImageSetVersionStr{};
        variable_field recordDescriptors{};
        variable_field fwDevicePkgData{};

        auto rc = decode_firmware_device_id_record(
            pkgHdr.data() + offset, pkgHdrRemainingSize,
            componentBitmapBitLength, &deviceIdRecHeader, &applicableComponents,
            &compImageSetVersionStr, &recordDescriptors, &fwDevicePkgData);
        if (rc)
        {
            error(
                "Failed to decode firmware device ID record, response code '{RC}'",
                "RC", rc);
            throw InternalFailure();
        }

        Descriptors descriptors{};
        while (deviceIdRecHeader.descriptor_count-- &&
               (recordDescriptors.length > 0))
        {
            uint16_t descriptorType = 0;
            variable_field descriptorData{};

            rc = decode_descriptor_type_length_value(
                recordDescriptors.ptr, recordDescriptors.length,
                &descriptorType, &descriptorData);
            if (rc)
            {
                error(
                    "Failed to decode descriptor type value of type '{TYPE}' and  length '{LENGTH}', response code '{RC}'",
                    "TYPE", descriptorType, "LENGTH", recordDescriptors.length,
                    "RC", rc);
                throw InternalFailure();
            }

            if (descriptorType != PLDM_FWUP_VENDOR_DEFINED)
            {
                descriptors.emplace(
                    descriptorType,
                    DescriptorData{descriptorData.ptr,
                                   descriptorData.ptr + descriptorData.length});
            }
            else
            {
                uint8_t descTitleStrType = 0;
                variable_field descTitleStr{};
                variable_field vendorDefinedDescData{};

                rc = decode_vendor_defined_descriptor_value(
                    descriptorData.ptr, descriptorData.length,
                    &descTitleStrType, &descTitleStr, &vendorDefinedDescData);
                if (rc)
                {
                    error(
                        "Failed to decode vendor-defined descriptor value of type '{TYPE}' and  length '{LENGTH}', response code '{RC}'",
                        "TYPE", descriptorType, "LENGTH",
                        recordDescriptors.length, "RC", rc);
                    throw InternalFailure();
                }

                descriptors.emplace(
                    descriptorType,
                    std::make_tuple(utils::toString(descTitleStr),
                                    VendorDefinedDescriptorData{
                                        vendorDefinedDescData.ptr,
                                        vendorDefinedDescData.ptr +
                                            vendorDefinedDescData.length}));
            }

            auto nextDescriptorOffset =
                sizeof(pldm_descriptor_tlv().descriptor_type) +
                sizeof(pldm_descriptor_tlv().descriptor_length) +
                descriptorData.length;
            recordDescriptors.ptr += nextDescriptorOffset;
            recordDescriptors.length -= nextDescriptorOffset;
        }

        DeviceUpdateOptionFlags deviceUpdateOptionFlags =
            deviceIdRecHeader.device_update_option_flags.value;

        ApplicableComponents componentsList;

        for (size_t varBitfieldIdx = 0;
             varBitfieldIdx < applicableComponents.length; varBitfieldIdx++)
        {
            std::bitset<8> entry{*(applicableComponents.ptr + varBitfieldIdx)};
            for (size_t idx = 0; idx < entry.size(); idx++)
            {
                if (entry[idx])
                {
                    componentsList.emplace_back(
                        idx + (varBitfieldIdx * entry.size()));
                }
            }
        }

        fwDeviceIDRecords.emplace_back(std::make_tuple(
            deviceUpdateOptionFlags, componentsList,
            utils::toString(compImageSetVersionStr), std::move(descriptors),
            FirmwareDevicePackageData{
                fwDevicePkgData.ptr,
                fwDevicePkgData.ptr + fwDevicePkgData.length}));
        offset += deviceIdRecHeader.record_length;
        pkgHdrRemainingSize -= deviceIdRecHeader.record_length;
    }

    return offset;
}

size_t PackageParser::parseCompImageInfoArea(ComponentImageCount compImageCount,
                                             const std::vector<uint8_t>& pkgHdr,
                                             size_t offset)
{
    size_t pkgHdrRemainingSize = pkgHdr.size() - offset;

    while (compImageCount-- && (pkgHdrRemainingSize > 0))
    {
        pldm_component_image_information compImageInfo{};
        variable_field compVersion{};

        auto rc = decode_pldm_comp_image_info(
            pkgHdr.data() + offset, pkgHdrRemainingSize, &compImageInfo,
            &compVersion);
        if (rc)
        {
            error(
                "Failed to decode component image information, response code '{RC}'",
                "RC", rc);
            throw InternalFailure();
        }

        CompClassification compClassification =
            compImageInfo.comp_classification;
        CompIdentifier compIdentifier = compImageInfo.comp_identifier;
        CompComparisonStamp compComparisonTime =
            compImageInfo.comp_comparison_stamp;
        CompOptions compOptions = compImageInfo.comp_options.value;
        ReqCompActivationMethod reqCompActivationMethod =
            compImageInfo.requested_comp_activation_method.value;
        CompLocationOffset compLocationOffset =
            compImageInfo.comp_location_offset;
        CompSize compSize = compImageInfo.comp_size;

        componentImageInfos.emplace_back(std::make_tuple(
            compClassification, compIdentifier, compComparisonTime, compOptions,
            reqCompActivationMethod, compLocationOffset, compSize,
            utils::toString(compVersion)));
        offset += sizeof(pldm_component_image_information) +
                  compImageInfo.comp_version_string_length;
        pkgHdrRemainingSize -= sizeof(pldm_component_image_information) +
                               compImageInfo.comp_version_string_length;
    }

    return offset;
}

void PackageParser::validatePkgTotalSize(uintmax_t pkgSize)
{
    uintmax_t calcPkgSize = pkgHeaderSize;
    for (const auto& componentImageInfo : componentImageInfos)
    {
        CompLocationOffset compLocOffset = std::get<static_cast<size_t>(
            ComponentImageInfoPos::CompLocationOffsetPos)>(componentImageInfo);
        CompSize compSize =
            std::get<static_cast<size_t>(ComponentImageInfoPos::CompSizePos)>(
                componentImageInfo);

        if (compLocOffset != calcPkgSize)
        {
            auto cmpVersion = std::get<static_cast<size_t>(
                ComponentImageInfoPos::CompVersionPos)>(componentImageInfo);
            error(
                "Failed to validate the component location offset '{OFFSET}' for version '{COMPONENT_VERSION}' and package size '{SIZE}'",
                "OFFSET", compLocOffset, "COMPONENT_VERSION", cmpVersion,
                "SIZE", calcPkgSize);
            throw InternalFailure();
        }

        calcPkgSize += compSize;
    }

    if (calcPkgSize != pkgSize)
    {
        error(
            "Failed to match package size '{PKG_SIZE}' to calculated package size '{CALCULATED_PACKAGE_SIZE}'.",
            "PKG_SIZE", pkgSize, "CALCULATED_PACKAGE_SIZE", calcPkgSize);
        throw InternalFailure();
    }
}

void PackageParserV1::parse(const std::vector<uint8_t>& pkgHdr,
                            uintmax_t pkgSize)
{
    if (pkgHeaderSize != pkgHdr.size())
    {
        error("Invalid package header size '{PKG_HDR_SIZE}' ", "PKG_HDR_SIZE",
              pkgHeaderSize);
        throw InternalFailure();
    }

    size_t offset = sizeof(pldm_package_header_information) + pkgVersion.size();
    if (offset + sizeof(DeviceIDRecordCount) >= pkgHeaderSize)
    {
        error("Failed to parse package header of size '{PKG_HDR_SIZE}'",
              "PKG_HDR_SIZE", pkgHeaderSize);
        throw InternalFailure();
    }

    auto deviceIdRecCount = static_cast<DeviceIDRecordCount>(pkgHdr[offset]);
    offset += sizeof(DeviceIDRecordCount);

    offset = parseFDIdentificationArea(deviceIdRecCount, pkgHdr, offset);
    if (deviceIdRecCount != fwDeviceIDRecords.size())
    {
        error("Failed to find DeviceIDRecordCount {DREC_CNT} entries",
              "DREC_CNT", deviceIdRecCount);
        throw InternalFailure();
    }
    if (offset + sizeof(ComponentImageCount) >= pkgHeaderSize)
    {
        error("Failed to parsing package header of size '{PKG_HDR_SIZE}'",
              "PKG_HDR_SIZE", pkgHeaderSize);
        throw InternalFailure();
    }

    auto compImageCount = static_cast<ComponentImageCount>(
        le16toh(pkgHdr[offset] | (pkgHdr[offset + 1] << 8)));
    offset += sizeof(ComponentImageCount);

    offset = parseCompImageInfoArea(compImageCount, pkgHdr, offset);
    if (compImageCount != componentImageInfos.size())
    {
        error("Failed to find ComponentImageCount '{COMP_IMG_CNT}' entries",
              "COMP_IMG_CNT", compImageCount);
        throw InternalFailure();
    }

    if (offset + sizeof(PackageHeaderChecksum) != pkgHeaderSize)
    {
        error("Failed to parse package header of size '{PKG_HDR_SIZE}'",
              "PKG_HDR_SIZE", pkgHeaderSize);
        throw InternalFailure();
    }

    auto calcChecksum = pldm_edac_crc32(pkgHdr.data(), offset);
    auto checksum = static_cast<PackageHeaderChecksum>(
        le32toh(pkgHdr[offset] | (pkgHdr[offset + 1] << 8) |
                (pkgHdr[offset + 2] << 16) | (pkgHdr[offset + 3] << 24)));
    if (calcChecksum != checksum)
    {
        error(
            "Failed to parse package header for calculated checksum '{CALCULATED_CHECKSUM}' and header checksum '{PACKAGE_HEADER_CHECKSUM}'",
            "CALCULATED_CHECKSUM", calcChecksum, "PACKAGE_HEADER_CHECKSUM",
            checksum);
        throw InternalFailure();
    }

    validatePkgTotalSize(pkgSize);
}

void PackageParserV130::parse(const std::vector<uint8_t>& pkgHdr,
                              uintmax_t pkgSize)
{
    if (pkgHeaderSize != pkgHdr.size())
    {
        error("Invalid package header size '{PKG_HDR_SIZE}' ", "PKG_HDR_SIZE",
              pkgHeaderSize);
        throw InternalFailure();
    }

    pldm_package_header_information_v130 packageHeaderInfoData{};
    pldm_firmware_device_id_record_v130_iter deviceIdRecIter{};
    pldm_downstream_device_id_record_v130_iter downstreamDeviceIdRecIter{};
    pldm_component_image_infornation_v130_iter compImageInfoIter{};
    auto rc = decode_pldm_package_header_info_v130(
        pkgHdr.data(), pkgHdr.size(), &packageHeaderInfoData, &deviceIdRecIter,
        &downstreamDeviceIdRecIter, &compImageInfoIter);
    if (rc)
    {
        error(
            "Failed to decode PLDM package header information, response code '{RC}'",
            "RC", rc);
        throw InternalFailure();
    }

    pldm_firmware_device_id_record_v130 deviceIdRec;
    variable_field descriptorData{};
    pldm_descriptor_iter descriptorIter{};
    descriptorIter.field = &descriptorData;

    foreach_pldm_firmware_device_id_record_v130(
        packageHeaderInfoData, deviceIdRecIter, deviceIdRec, descriptorIter, rc)
    {
        pldm_descriptor descriptorData{};
        Descriptors descriptors{};
        foreach_pldm_descriptor(descriptorIter, descriptorData, rc)
        {
            auto descriptorType = descriptorData.descriptor_type;
            const auto descriptorDataPtr =
                new (const_cast<void*>(descriptorData.descriptor_data))
                    uint8_t[descriptorData.descriptor_length];
            auto descriptorDataLength = descriptorData.descriptor_length;
            if (descriptorType != PLDM_FWUP_VENDOR_DEFINED)
            {
                descriptors.emplace(
                    descriptorType,
                    DescriptorData{descriptorDataPtr,
                                   descriptorDataPtr + descriptorDataLength});
            }
            else
            {
                uint8_t descTitleStrType = 0;
                variable_field descTitleStr{};
                variable_field vendorDefinedDescData{};
                rc = decode_vendor_defined_descriptor_value(
                    descriptorDataPtr, descriptorDataLength, &descTitleStrType,
                    &descTitleStr, &vendorDefinedDescData);
                if (rc)
                {
                    error(
                        "Failed to decode vendor-defined descriptor value of type '{TYPE}' and  length '{LENGTH}', response code '{RC}'",
                        "TYPE", descriptorType, "LENGTH",
                        deviceIdRec.record_length, "RC", rc);
                    throw InternalFailure();
                }

                descriptors.emplace(
                    descriptorType,
                    std::make_tuple(utils::toString(descTitleStr),
                                    VendorDefinedDescriptorData{
                                        vendorDefinedDescData.ptr,
                                        vendorDefinedDescData.ptr +
                                            vendorDefinedDescData.length}));
            }
        }
        if (rc)
        {
            error("Failed to decode descriptor type, response code '{RC}'",
                  "RC", rc);
            throw InternalFailure();
        }

        DeviceUpdateOptionFlags deviceUpdateOptionFlags =
            deviceIdRec.device_update_option_flags.value;
        ApplicableComponents applicableComponents;
        for (size_t varBitfieldIdx = 0;
             varBitfieldIdx < deviceIdRec.applicable_components.length;
             varBitfieldIdx++)
        {
            std::bitset<8> entry{
                *(deviceIdRec.applicable_components.ptr + varBitfieldIdx)};
            for (size_t idx = 0; idx < entry.size(); idx++)
            {
                if (entry[idx])
                {
                    applicableComponents.emplace_back(
                        idx + (varBitfieldIdx * entry.size()));
                }
            }
        }

        const auto packageVersionStringPtr = new (
            const_cast<void*>(deviceIdRec.component_image_set_version_string))
            uint8_t[deviceIdRec.component_image_set_version_string_length];
        const auto packageVersionStringLength =
            deviceIdRec.component_image_set_version_string_length;
        std::string compImageSetVersion(
            packageVersionStringPtr,
            packageVersionStringPtr + packageVersionStringLength);

        const auto fwDevicePkgDataPtr =
            new (const_cast<void*>(deviceIdRec.firmware_device_package_data))
                uint8_t[deviceIdRec.firmware_device_package_data_length];
        const auto fwDevicePkgDataLength =
            deviceIdRec.firmware_device_package_data_length;

        fwDeviceIDRecords.emplace_back(std::make_tuple(
            deviceUpdateOptionFlags, applicableComponents,
            std::move(compImageSetVersion), std::move(descriptors),
            FirmwareDevicePackageData{
                fwDevicePkgDataPtr,
                fwDevicePkgDataPtr + fwDevicePkgDataLength}));
    }
    if (rc)
    {
        error(
            "Failed to decode firmware device ID record, response code '{RC}'",
            "RC", rc);
        throw InternalFailure();
    }
    pldm_downstream_device_id_record_v130 downstreamDeviceIdRec;
    foreach_pldm_downstream_device_id_record_v130(
        packageHeaderInfoData, downstreamDeviceIdRecIter, downstreamDeviceIdRec,
        descriptorIter, rc)
    {
        Descriptors descriptors{};
        pldm_descriptor descriptorData{};
        foreach_pldm_descriptor(descriptorIter, descriptorData, rc)
        {
            auto descriptorType = descriptorData.descriptor_type;
            const auto descriptorDataPtr =
                new (const_cast<void*>(descriptorData.descriptor_data))
                    uint8_t[descriptorData.descriptor_length];
            auto descriptorDataLength = descriptorData.descriptor_length;
            if (descriptorType != PLDM_FWUP_VENDOR_DEFINED)
            {
                descriptors.emplace(
                    descriptorType,
                    DescriptorData{descriptorDataPtr,
                                   descriptorDataPtr + descriptorDataLength});
            }
            else
            {
                uint8_t descTitleStrType = 0;
                variable_field descTitleStr{};
                variable_field vendorDefinedDescData{};

                rc = decode_vendor_defined_descriptor_value(
                    descriptorDataPtr, descriptorDataLength, &descTitleStrType,
                    &descTitleStr, &vendorDefinedDescData);
                if (rc)
                {
                    error(
                        "Failed to decode vendor-defined descriptor value of type '{TYPE}' and  length '{LENGTH}', response code '{RC}'",
                        "TYPE", descriptorType, "LENGTH",
                        downstreamDeviceIdRec.downstream_device_record_length,
                        "RC", rc);
                    throw InternalFailure();
                }

                descriptors.emplace(
                    descriptorType,
                    std::make_tuple(utils::toString(descTitleStr),
                                    VendorDefinedDescriptorData{
                                        vendorDefinedDescData.ptr,
                                        vendorDefinedDescData.ptr +
                                            vendorDefinedDescData.length}));
            }
        }
        if (rc)
        {
            error(
                "Failed to decode downstream descriptor type, response code '{RC}'",
                "RC", rc);
            throw InternalFailure();
        }

        DeviceUpdateOptionFlags deviceUpdateOptionFlags =
            downstreamDeviceIdRec.downstream_device_update_option_flags.value;
        ApplicableComponents applicableComponents;
        for (size_t varBitfieldIdx = 0;
             varBitfieldIdx < deviceIdRec.applicable_components.length;
             varBitfieldIdx++)
        {
            std::bitset<8> entry{
                *(deviceIdRec.applicable_components.ptr + varBitfieldIdx)};
            for (size_t idx = 0; idx < entry.size(); idx++)
            {
                if (entry[idx])
                {
                    applicableComponents.emplace_back(
                        idx + (varBitfieldIdx * entry.size()));
                }
            }
        }

        const auto packageVersionStringPtr = new (const_cast<void*>(
            downstreamDeviceIdRec
                .downstream_device_self_contained_activation_min_version_string)) uint8_t
            [downstreamDeviceIdRec
                 .downstream_device_self_contained_activation_min_version_string_length];
        const auto packageVersionStringLength =
            downstreamDeviceIdRec
                .downstream_device_self_contained_activation_min_version_string_length;

        std::string compImageSetVersion(
            packageVersionStringPtr,
            packageVersionStringPtr + packageVersionStringLength);

        const auto downstreamDevicePkgDataPtr = new (const_cast<void*>(
            downstreamDeviceIdRec.downstream_device_package_data))
            uint8_t[downstreamDeviceIdRec
                        .downstream_device_package_data_length];
        const auto downstreamDevicePkgDataLength =
            downstreamDeviceIdRec.downstream_device_package_data_length;

        downstreamDeviceIDRecords.emplace_back(std::make_tuple(
            deviceUpdateOptionFlags, applicableComponents,
            std::move(compImageSetVersion), std::move(descriptors),
            FirmwareDevicePackageData{
                downstreamDevicePkgDataPtr,
                downstreamDevicePkgDataPtr + downstreamDevicePkgDataLength}));
    }
    if (rc)
    {
        error(
            "Failed to decode downstream device ID record, response code '{RC}'",
            "RC", rc);
        throw InternalFailure();
    }
    pldm_component_image_information_v130 compImageInfo;
    foreach_pldm_component_image_information_v130(compImageInfoIter,
                                                  compImageInfo, rc)
    {
        CompClassification compClassification =
            compImageInfo.component_classification;
        CompIdentifier compIdentifier = compImageInfo.component_identifier;
        CompComparisonStamp compComparisonTime =
            compImageInfo.component_comparison_stamp;
        CompOptions compOptions = compImageInfo.component_options.value;
        ReqCompActivationMethod reqCompActivationMethod =
            compImageInfo.requested_component_activation_method.value;
        CompLocationOffset compLocationOffset =
            compImageInfo.component_location_offset;
        CompSize compSize = compImageInfo.component_size;

        const auto compVersionStringPtr =
            new (const_cast<void*>(compImageInfo.component_version_string))
                uint8_t[compImageInfo.component_version_string_length];
        const auto compVersionStringLength =
            compImageInfo.component_version_string_length;
        std::string compVersion(compVersionStringPtr,
                                compVersionStringPtr + compVersionStringLength);
        componentImageInfos.emplace_back(std::make_tuple(
            compClassification, compIdentifier, compComparisonTime, compOptions,
            reqCompActivationMethod, compLocationOffset, compSize,
            std::move(compVersion)));
    }
    if (rc)
    {
        error(
            "Failed to decode component image information, response code '{RC}'",
            "RC", rc);
        throw InternalFailure();
    }
    auto headerChecksum = packageHeaderInfoData.package_header_checksum;
    auto payloadChecksum = packageHeaderInfoData.package_payload_checksum;
    auto headerSize = packageHeaderInfoData.package_header_size;
    auto calcHeaderChecksum = pldm_edac_crc32(
        pkgHdr.data(),
        headerSize - sizeof(headerChecksum) - sizeof(payloadChecksum));
    if (calcHeaderChecksum != headerChecksum)
    {
        error(
            "Failed to parse package header for calculated checksum '{CALCULATED_CHECKSUM}' and header checksum '{PACKAGE_HEADER_CHECKSUM}'",
            "CALCULATED_CHECKSUM", calcHeaderChecksum,
            "PACKAGE_HEADER_CHECKSUM", headerChecksum);
        throw InternalFailure();
    }
    auto calcPayloadChecksum =
        pldm_edac_crc32(pkgHdr.data() + headerSize, pkgSize - headerSize);
    if (calcPayloadChecksum != payloadChecksum)
    {
        error(
            "Failed to parse package payload for calculated checksum '{CALCULATED_CHECKSUM}' and payload checksum '{PACKAGE_HEADER_CHECKSUM}'",
            "CALCULATED_CHECKSUM", calcPayloadChecksum,
            "PACKAGE_HEADER_CHECKSUM", payloadChecksum);
        throw InternalFailure();
    }
}

std::unique_ptr<PackageParser> parsePkgHeader(std::vector<uint8_t>& pkgData)
{
    constexpr std::array<uint8_t, PLDM_FWUP_UUID_LENGTH> hdrIdentifierv1{
        0xF0, 0x18, 0x87, 0x8C, 0xCB, 0x7D, 0x49, 0x43,
        0x98, 0x00, 0xA0, 0x2F, 0x05, 0x9A, 0xCA, 0x02};
    constexpr std::array<uint8_t, PLDM_FWUP_UUID_LENGTH> hdrIdentifierv130{
        0x7B, 0x29, 0x1C, 0x99, 0x6D, 0xB6, 0x42, 0x08,
        0x80, 0x1B, 0x02, 0x02, 0x6E, 0x46, 0x3C, 0x78};
    constexpr uint8_t pkgHdrVersion1 = 0x01;
    constexpr uint8_t pkgHdrVersion130 = 0x04;

    pldm_package_header_information pkgHeader{};
    variable_field pkgVersion{};
    auto rc = decode_pldm_package_header_info(pkgData.data(), pkgData.size(),
                                              &pkgHeader, &pkgVersion);
    if (rc)
    {
        error(
            "Failed to decode PLDM package header information, response code '{RC}'",
            "RC", rc);
        return nullptr;
    }

    if (std::equal(pkgHeader.uuid, pkgHeader.uuid + PLDM_FWUP_UUID_LENGTH,
                   hdrIdentifierv1.begin(), hdrIdentifierv1.end()) &&
        (pkgHeader.package_header_format_version == pkgHdrVersion1))
    {
        PackageHeaderSize pkgHdrSize = pkgHeader.package_header_size;
        ComponentBitmapBitLength componentBitmapBitLength =
            pkgHeader.component_bitmap_bit_length;
        return std::make_unique<PackageParserV1>(
            pkgHdrSize, utils::toString(pkgVersion), componentBitmapBitLength);
    }
    else if (std::equal(pkgHeader.uuid, pkgHeader.uuid + PLDM_FWUP_UUID_LENGTH,
                        hdrIdentifierv130.begin(), hdrIdentifierv130.end()) &&
             (pkgHeader.package_header_format_version == pkgHdrVersion130))
    {
        PackageHeaderSize pkgHdrSize = pkgHeader.package_header_size;
        ComponentBitmapBitLength componentBitmapBitLength =
            pkgHeader.component_bitmap_bit_length;
        return std::make_unique<PackageParserV130>(
            pkgHdrSize, utils::toString(pkgVersion), componentBitmapBitLength);
    }

    return nullptr;
}

} // namespace fw_update

} // namespace pldm
