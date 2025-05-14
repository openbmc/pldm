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

void PackageParser::parse(const std::vector<uint8_t>& pkgHdr)
{
    pldm_package_header_information_pad hdr{};
    pldm_package_iter packageItr{};
    DEFINE_PLDM_PACKAGE_FORMAT_PIN(pin);

    int rc = 0;
    rc = decode_pldm_firmware_update_package(pkgHdr.data(), pkgHdr.size(), &pin,
                                             &hdr, &packageItr);
    if (rc)
    {
        error(
            "Failed to decode PLDM package header information to iterator, response code '{RC}'",
            "RC", rc);
        throw InternalFailure();
    }
    auto packageHeaderInfoData = hdr;
    pkgVersion = utils::toString(packageHeaderInfoData.package_version_string);
    componentBitmapBitLength =
        packageHeaderInfoData.component_bitmap_bit_length;

    pldm_package_firmware_device_id_record deviceIdRecordData{};

    foreach_pldm_package_firmware_device_id_record(packageItr,
                                                   deviceIdRecordData, rc)
    {
        pldm_descriptor descriptorData{};
        Descriptors descriptors{};
        foreach_pldm_package_firmware_device_id_record_descriptor(
            packageItr, deviceIdRecordData, descriptorData, rc)
        {
            auto descriptorType = descriptorData.descriptor_type;
            const auto descriptorDataPtr =
                static_cast<const uint8_t*>(descriptorData.descriptor_data);
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
                        vendorDefinedDescData.length, "RC", rc);
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
            deviceIdRecordData.device_update_option_flags.value;
        ApplicableComponents applicableComponents;
        for (size_t varBitfieldIdx = 0;
             varBitfieldIdx <
             deviceIdRecordData.applicable_components.bitmap.length;
             varBitfieldIdx++)
        {
            std::bitset<8> entry{
                *(deviceIdRecordData.applicable_components.bitmap.ptr +
                  varBitfieldIdx)};
            for (size_t idx = 0; idx < entry.size(); idx++)
            {
                if (entry[idx])
                {
                    applicableComponents.emplace_back(
                        idx + (varBitfieldIdx * entry.size()));
                }
            }
        }
        auto compImageSetVersion = utils::toString(
            deviceIdRecordData.component_image_set_version_string);
        FirmwareDevicePackageData fwDevicePkgData{
            deviceIdRecordData.firmware_device_package_data.ptr,
            deviceIdRecordData.firmware_device_package_data.ptr +
                deviceIdRecordData.firmware_device_package_data.length};
        fwDeviceIDRecords.emplace_back(std::make_tuple(
            deviceUpdateOptionFlags, applicableComponents,
            std::move(compImageSetVersion), std::move(descriptors),
            std::move(fwDevicePkgData)));
    }
    if (rc)
    {
        error(
            "Failed to decode firmware device ID record, response code '{RC}'",
            "RC", rc);
        throw InternalFailure();
    }

    pldm_package_downstream_device_id_record downstreamDeviceIdRec{};
    foreach_pldm_package_downstream_device_id_record(packageItr,
                                                     downstreamDeviceIdRec, rc)
    {
        Descriptors descriptors{};
        pldm_descriptor descriptorData{};
        foreach_pldm_package_downstream_device_id_record_descriptor(
            packageItr, downstreamDeviceIdRec, descriptorData, rc)
        {
            auto descriptorType = descriptorData.descriptor_type;
            const auto descriptorDataPtr =
                static_cast<const uint8_t*>(descriptorData.descriptor_data);
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
                        vendorDefinedDescData.length, "RC", rc);
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
            downstreamDeviceIdRec.update_option_flags.value;
        ApplicableComponents applicableComponents;
        for (size_t varBitfieldIdx = 0;
             varBitfieldIdx <
             downstreamDeviceIdRec.applicable_components.bitmap.length;
             varBitfieldIdx++)
        {
            std::bitset<8> entry{
                *(downstreamDeviceIdRec.applicable_components.bitmap.ptr +
                  varBitfieldIdx)};
            for (size_t idx = 0; idx < entry.size(); idx++)
            {
                if (entry[idx])
                {
                    applicableComponents.emplace_back(
                        idx + (varBitfieldIdx * entry.size()));
                }
            }
        }
        SelfContainedActivationMinVersion selfContainedActivationMinVersion =
            std::nullopt;
        SelfContainedActivationMinVersionComparisonStamp
            selfContainedActivationMinVersionComparisonStamp = std::nullopt;
        if (deviceUpdateOptionFlags[0])
        {
            selfContainedActivationMinVersion = utils::toString(
                downstreamDeviceIdRec
                    .self_contained_activation_min_version_string);
            selfContainedActivationMinVersionComparisonStamp =
                downstreamDeviceIdRec
                    .self_contained_activation_min_version_comparison_stamp;
        }
        FirmwareDevicePackageData fwDevicePkgData{
            downstreamDeviceIdRec.package_data.ptr,
            downstreamDeviceIdRec.package_data.ptr +
                downstreamDeviceIdRec.package_data.length};
        downstreamDeviceIDRecords.emplace_back(std::make_tuple(
            deviceUpdateOptionFlags, applicableComponents,
            std::move(selfContainedActivationMinVersion),
            std::move(selfContainedActivationMinVersionComparisonStamp),
            std::move(descriptors), std::move(fwDevicePkgData)));
    }
    if (rc)
    {
        error(
            "Failed to decode downstream device ID record, response code '{RC}'",
            "RC", rc);
        throw InternalFailure();
    }

    pldm_package_component_image_information compImageInfo{};
    foreach_pldm_package_component_image_information(packageItr, compImageInfo,
                                                     rc)
    {
        componentImageInfos.emplace_back(
            compImageInfo,
            CompImage{compImageInfo.component_image.ptr,
                      compImageInfo.component_image.ptr +
                          compImageInfo.component_image.length},
            utils::toString(compImageInfo.component_version_string));
    }
    if (rc)
    {
        error(
            "Failed to decode component image information, response code '{RC}'",
            "RC", rc);
        throw InternalFailure();
    }
}

} // namespace fw_update

} // namespace pldm
