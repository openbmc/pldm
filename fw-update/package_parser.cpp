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

void PackageParser::parseFDIdentificationArea()
{
    pldm_firmware_device_id_record deviceIdRecHeader{};
    variable_field applicableComponents{};
    variable_field compImageSetVersionStr{};
    variable_field recordDescriptors{};
    variable_field fwDevicePkgData{};
    int rc = 0;

    foreach_pldm_fw_device_record(
        this->pkgIter.dev_iter, &deviceIdRecHeader, &applicableComponents,
        &compImageSetVersionStr, &recordDescriptors, &fwDevicePkgData, rc)
    {
        if (rc)
        {
            error(
                "Failed to decode firmware device ID record, response code '{RC}'",
                "RC", rc);
            throw InternalFailure();
        }

        DeviceIDRecordCount descCount = deviceIdRecHeader.descriptor_count;
        Descriptors descriptors{};
        uint16_t descriptorType = 0;
        variable_field descriptorData{};
        variable_field descTitleStr{};
        variable_field vendorDefinedDescData{};
        int rc = 0;

        foreach_pldm_descriptor(recordDescriptors, descCount, descriptorType,
                                descriptorData, descTitleStr,
                                vendorDefinedDescData, rc);
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
            descriptors.emplace(
                descriptorType,
                std::make_tuple(utils::toString(descTitleStr),
                                VendorDefinedDescriptorData{
                                    vendorDefinedDescData.ptr,
                                    vendorDefinedDescData.ptr +
                                        vendorDefinedDescData.length}));
        }
    }

    DeviceUpdateOptionFlags deviceUpdateOptionFlags =
        deviceIdRecHeader.device_update_option_flags.value;

    ApplicableComponents componentsList{};

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

    fwDeviceIDRecords.emplace_back(deviceUpdateOptionFlags,
                                  std::move(componentsList),
                                  utils::toString(compImageSetVersionStr),
                                  std::move(descriptors),
                                  FirmwareDevicePackageData(fwDevicePkgData.ptr,
                                                          fwDevicePkgData.ptr + 
                                                              fwDevicePkgData.length));
}

void PackageParser::parseCompImageInfoArea()
{
    pldm_component_image_information compImageInfo{};
    variable_field compVersion{};
    int rc = 0;

    componentImageInfos{};
    foreach_pldm_comp_image_record(this->pkgIter.comp_iter, compImageInfo,
                                   compVersion, rc)
    {
        if (rc)
        {
            error(
                "Failed to decode component image information, response code '{RC}'",
                "RC", rc);
            throw InternalFailure();
        }

        componentImageInfos.emplace_back(compImageInfo.comp_classification,
                                       compImageInfo.comp_identifier,
                                       compImageInfo.comp_comparison_stamp,
                                       compImageInfo.comp_options.value,
                                       compImageInfo.requested_comp_activation_method.value,
                                       compImageInfo.comp_location_offset,
                                       compImageInfo.comp_size,
                                       utils::toString(compVersion));
    }
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
    const int rc =
        pldm_fwup_pkg_iter_init(pkgHdr.data(), pkgHdr.size(), &this->pkgIter);
    if (rc)
    {
        error("Failed to init pldm_fwup_pkg_iter", "rc", rc);
        throw InternalFailure();
    }
    if (!this->pkgIter.header)
    {
        error("Invalid package header");
        throw InternalFailure();
    }

    this->pkgHeaderSize = this->pkgIter.header->package_header_size;
    this->componentBitmapBitLength =
        this->pkgIter.header->component_bitmap_bit_length;

    if (this->pkgIter.pkg_version.ptr && this->pkgIter.pkg_version.length)
    {
        this->pkgVersion = std::string_view(
            reinterpret_cast<const char*>(this->pkgIter.pkg_version.ptr),
            this->pkgIter.pkg_version.length);
    }

    parseFDIdentificationArea();
    parseCompImageInfoArea();
    validatePkgTotalSize(pkgSize);
}

} // namespace fw_update

} // namespace pldm
