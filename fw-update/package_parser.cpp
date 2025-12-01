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

ApplicableComponents PackageParser::parseBitmapToComponents(
    const uint8_t* bitmapPtr, size_t bitmapLength)
{
    std::span<const uint8_t> bitmap(bitmapPtr, bitmapLength);
    constexpr size_t bitsPerByte = 8;
    ApplicableComponents components;

    components.reserve(bitmapLength * bitsPerByte);

    for (size_t byteIndex = 0; byteIndex < bitmap.size(); byteIndex++)
    {
        std::bitset<bitsPerByte> bits{bitmap[byteIndex]};

        for (size_t bitIndex = 0; bitIndex < bitsPerByte; bitIndex++)
        {
            if (bits[bitIndex])
            {
                components.emplace_back(bitIndex + (byteIndex * bitsPerByte));
            }
        }
    }

    return components;
}

void PackageParser::parse(std::span<const uint8_t> package)
{
    pldm_package_downstream_device_id_record downstreamDeviceIdRecord{};
    pldm_package_firmware_device_id_record firmwareDeviceIdRecord{};
    pldm_package_component_image_information componentImageInformation{};
    DEFINE_PLDM_PACKAGE_FORMAT_PIN_FR04H(pin);
    pldm_package_header_information_pad hdr{};
    pldm_package pkg{};
    int rc = 0;

    rc = decode_pldm_firmware_update_package(package.data(), package.size(),
                                             &pin, &hdr, &pkg, 0);
    if (rc)
    {
        error(
            "Failed to decode PLDM package header information to iterator, response code '{RC}'",
            "RC", rc);
        throw InternalFailure();
    }

    pkgVersion = utils::toString(hdr.package_version_string);

    foreach_pldm_package_firmware_device_id_record(pkg, firmwareDeviceIdRecord,
                                                   rc)
    {
        pldm_descriptor descriptorData{};
        Descriptors descriptors{};

        foreach_pldm_package_firmware_device_id_record_descriptor(
            pkg, firmwareDeviceIdRecord, descriptorData, rc)
        {
            const auto descriptorDataPtr =
                static_cast<const uint8_t*>(descriptorData.descriptor_data);
            auto descriptorLength = descriptorData.descriptor_length;
            auto descriptorType = descriptorData.descriptor_type;

            if (descriptorType != PLDM_FWUP_VENDOR_DEFINED)
            {
                descriptors.emplace(
                    descriptorType,
                    DescriptorData{descriptorDataPtr,
                                   descriptorDataPtr + descriptorLength});
            }
            else
            {
                variable_field vendorDefinedDescriptorData{};
                variable_field descriptorTitleStr{};
                uint8_t descriptorTitleStrType = 0;

                rc = decode_vendor_defined_descriptor_value(
                    descriptorDataPtr, descriptorLength,
                    &descriptorTitleStrType, &descriptorTitleStr,
                    &vendorDefinedDescriptorData);
                if (rc)
                {
                    error(
                        "Failed to decode vendor-defined descriptor value of type '{TYPE}' and  length '{LENGTH}', response code '{RC}'",
                        "TYPE", descriptorType, "LENGTH",
                        vendorDefinedDescriptorData.length, "RC", rc);
                    throw InternalFailure();
                }

                descriptors.emplace(
                    descriptorType,
                    std::make_tuple(
                        utils::toString(descriptorTitleStr),
                        VendorDefinedDescriptorData{
                            vendorDefinedDescriptorData.ptr,
                            vendorDefinedDescriptorData.ptr +
                                vendorDefinedDescriptorData.length}));
            }
        }
        if (rc)
        {
            error("Failed to decode descriptor type, response code '{RC}'",
                  "RC", rc);
            throw InternalFailure();
        }

        // Extract firmware device ID record fields
        DeviceUpdateOptionFlags deviceUpdateOptionFlags =
            firmwareDeviceIdRecord.device_update_option_flags.value;
        ApplicableComponents applicableComponents = parseBitmapToComponents(
            firmwareDeviceIdRecord.applicable_components.bitmap.ptr,
            firmwareDeviceIdRecord.applicable_components.bitmap.length);
        auto compImageSetVersion = utils::toString(
            firmwareDeviceIdRecord.component_image_set_version_string);
        FirmwareDevicePackageData fwDevicePkgData{
            firmwareDeviceIdRecord.firmware_device_package_data.ptr,
            firmwareDeviceIdRecord.firmware_device_package_data.ptr +
                firmwareDeviceIdRecord.firmware_device_package_data.length};

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

    foreach_pldm_package_downstream_device_id_record(
        pkg, downstreamDeviceIdRecord, rc)
    {
        pldm_descriptor descriptorData{};
        Descriptors descriptors{};

        foreach_pldm_package_downstream_device_id_record_descriptor(
            pkg, downstreamDeviceIdRecord, descriptorData, rc)
        {
            const auto descriptorDataPtr =
                static_cast<const uint8_t*>(descriptorData.descriptor_data);
            auto descriptorLength = descriptorData.descriptor_length;
            auto descriptorType = descriptorData.descriptor_type;

            if (descriptorType != PLDM_FWUP_VENDOR_DEFINED)
            {
                descriptors.emplace(
                    descriptorType,
                    DescriptorData{descriptorDataPtr,
                                   descriptorDataPtr + descriptorLength});
            }
            else
            {
                variable_field vendorDefinedDescriptorData{};
                variable_field descriptorTitleStr{};
                uint8_t descriptorTitleStrType = 0;

                rc = decode_vendor_defined_descriptor_value(
                    descriptorDataPtr, descriptorLength,
                    &descriptorTitleStrType, &descriptorTitleStr,
                    &vendorDefinedDescriptorData);
                if (rc)
                {
                    error(
                        "Failed to decode vendor-defined descriptor value of type '{TYPE}' and  length '{LENGTH}', response code '{RC}'",
                        "TYPE", descriptorType, "LENGTH",
                        vendorDefinedDescriptorData.length, "RC", rc);
                    throw InternalFailure();
                }

                descriptors.emplace(
                    descriptorType,
                    std::make_tuple(
                        utils::toString(descriptorTitleStr),
                        VendorDefinedDescriptorData{
                            vendorDefinedDescriptorData.ptr,
                            vendorDefinedDescriptorData.ptr +
                                vendorDefinedDescriptorData.length}));
            }
        }
        if (rc)
        {
            error("Failed to decode descriptor type, response code '{RC}'",
                  "RC", rc);
            throw InternalFailure();
        }

        // Extract downstream device ID record fields
        DeviceUpdateOptionFlags deviceUpdateOptionFlags =
            downstreamDeviceIdRecord.update_option_flags.value;
        ApplicableComponents applicableComponents = parseBitmapToComponents(
            downstreamDeviceIdRecord.applicable_components.bitmap.ptr,
            downstreamDeviceIdRecord.applicable_components.bitmap.length);

        // Optional self-contained activation info
        SelfContainedActivationMinVersion selfContainedActivationMinVersion =
            std::nullopt;
        SelfContainedActivationMinVersionComparisonStamp
            selfContainedActivationMinVersionComparisonStamp = std::nullopt;

        if (deviceUpdateOptionFlags[0])
        {
            selfContainedActivationMinVersion = utils::toString(
                downstreamDeviceIdRecord
                    .self_contained_activation_min_version_string);
            selfContainedActivationMinVersionComparisonStamp =
                downstreamDeviceIdRecord
                    .self_contained_activation_min_version_comparison_stamp;
        }
        FirmwareDevicePackageData fwDevicePkgData{
            downstreamDeviceIdRecord.package_data.ptr,
            downstreamDeviceIdRecord.package_data.ptr +
                downstreamDeviceIdRecord.package_data.length};

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

    foreach_pldm_package_component_image_information(pkg,
                                                     firmwareDeviceIdRecord, rc)
    {
        componentImageInfos.emplace_back(
            firmwareDeviceIdRecord,
            CompImage{firmwareDeviceIdRecord.component_image.ptr,
                      firmwareDeviceIdRecord.component_image.ptr +
                          firmwareDeviceIdRecord.component_image.length},
            utils::toString(firmwareDeviceIdRecord.component_version_string));
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
