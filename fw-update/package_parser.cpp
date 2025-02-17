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
    pldm_firmware_device_id_record_api fdrec{};
    int rc = 0;

    info("Starting to parse firmware device identification area");

    foreach_pldm_firmware_device_id_record(this->pkgIter, fdrec, rc)
    {
        if (rc)
        {
            error("Failed to decode firmware device ID record, rc = {RC}", "RC",
                  rc);
            throw InternalFailure();
        }

        auto compImageSetVersionStr = fdrec.component_image_set_version_string;
        auto deviceUpdateOptionFlags = fdrec.device_update_option_flags.value;

        ApplicableComponents componentsList;
        const size_t length = fdrec.applicable_components.bitmap.length;
        const uint8_t* bitsPtr = fdrec.applicable_components.bitmap.ptr;

        for (size_t b = 0; b < length; ++b)
        {
            std::bitset<8> bitsetByte(*(bitsPtr + b));
            for (size_t bitPos = 0; bitPos < bitsetByte.size(); ++bitPos)
            {
                if (bitsetByte.test(bitPos))
                {
                    componentsList.emplace_back(b * 8 + bitPos);
                }
            }
        }

        Descriptors descriptors;
        {
            struct pldm_descriptor desc{};
            int rcDesc = 0;

            foreach_pldm_firmware_device_id_record_descriptor(
                this->pkgIter, fdrec, desc, rcDesc)
            {
                if (rcDesc)
                {
                    error("Failed decoding FD record descriptor, rc = {RC}",
                          "RC", rcDesc);
                    throw InternalFailure();
                }
                const auto* dataPtr =
                    static_cast<const uint8_t*>(desc.descriptor_data);
                std::vector<uint8_t> descBytes(
                    dataPtr, dataPtr + desc.descriptor_length);

                descriptors.emplace(desc.descriptor_type, std::move(descBytes));
                // if (desc.descriptor_type != PLDM_FWUP_VENDOR_DEFINED)
                // {
                //     descriptors.emplace(
                //         desc.descriptor_type,
                //         DescriptorData{desc.descriptor_data,
                //                        desc.descriptor_data +
                //                            desc.descriptor_length});
                // }
                // else
                // {
                //     descriptors.emplace(
                //         desc.descriptor_type,
                //         DescriptorData{desc.descriptor_data,
                //                        desc.descriptor_data +
                //                            desc.descriptor_length});
                // }
            }
        }

        auto fwDevicePkgData = fdrec.firmware_device_package_data;

        fwDeviceIDRecords.emplace_back(
            deviceUpdateOptionFlags, std::move(componentsList),
            utils::toString(compImageSetVersionStr), std::move(descriptors),
            FirmwareDevicePackageData{
                fwDevicePkgData.ptr,
                fwDevicePkgData.ptr + fwDevicePkgData.length});
    }

    info("Total firmware device ID records parsed: {COUNT}", "COUNT",
         fwDeviceIDRecords.size());
}

void PackageParser::parseCompImageInfoArea()
{
    pldm_component_image_information_api compInfo{};
    int rc = 0;

    foreach_pldm_component_image_information(this->pkgIter, compInfo, rc)
    {
        if (rc)
        {
            error("Failed to decode component image information, rc = {RC}",
                  "RC", rc);
            throw InternalFailure();
        }

        const auto classification = compInfo.component_classification;
        const auto identifier = compInfo.component_identifier;
        const auto compStamp = compInfo.component_comparison_stamp;
        const auto options = compInfo.component_options.value;
        const auto activationMethod =
            compInfo.requested_component_activation_method.value;
        const auto locOffset = compInfo.component_location_offset;
        const auto size = compInfo.component_size;
        const auto versionStr =
            utils::toString(compInfo.component_version_string);

        componentImageInfos.emplace_back(classification, identifier, compStamp,
                                         options, activationMethod, locOffset,
                                         size, versionStr);
    }
}

void PackageParser::validatePkgTotalSize(uintmax_t pkgSize)
{
    if (!this->pkgIter.package.ptr || !this->pkgIter.hdr.images.ptr)
    {
        error("Invalid pointers in package header for size validation");
        throw InternalFailure();
    }

    const uint8_t* pkgStart =
        static_cast<const uint8_t*>(this->pkgIter.package.ptr);
    const uint8_t* imgStart =
        static_cast<const uint8_t*>(this->pkgIter.hdr.images.ptr);

    if (imgStart < pkgStart)
    {
        error("Corrupted iteration pointers: images pointer < package pointer");
        throw InternalFailure();
    }

    const uintmax_t pkgHeaderSize = static_cast<uintmax_t>(imgStart - pkgStart);
    uintmax_t calcPkgSize = pkgHeaderSize;

    for (const auto& comp : componentImageInfos)
    {
        const auto offset = std::get<static_cast<size_t>(
            ComponentImageInfoPos::CompLocationOffsetPos)>(comp);
        const auto size =
            std::get<static_cast<size_t>(ComponentImageInfoPos::CompSizePos)>(
                comp);
        const auto version = std::get<static_cast<size_t>(
            ComponentImageInfoPos::CompVersionPos)>(comp);

        if (offset != calcPkgSize)
        {
            error(
                "Mismatch in component offset: expected {EXP}, got {ACT}; comp version: {VER}",
                "EXP", calcPkgSize, "ACT", offset, "VER", version);
            throw InternalFailure();
        }
        calcPkgSize += size;
    }

    if (calcPkgSize != pkgSize)
    {
        error("Package size mismatch: actual = {ACTUAL}, expected = {EXPECTED}",
              "ACTUAL", pkgSize, "EXPECTED", calcPkgSize);
        throw InternalFailure();
    }
}

void PackageParserV1::parse(const std::vector<uint8_t>& pkgHdr,
                            uintmax_t pkgSize)
{
    int rc = decode_pldm_firmware_update_package(pkgHdr.data(), pkgHdr.size(),
                                                 &this->pkgIter);
    if (rc)
    {
        error("decode_pldm_firmware_update_package() failed, rc = {RC}", "RC",
              rc);
        throw InternalFailure();
    }

    auto& hdr = this->pkgIter.hdr;

    if (hdr.package_version_string.ptr && hdr.package_version_string.length)
    {
        this->pkgVersion = std::string_view(
            reinterpret_cast<const char*>(hdr.package_version_string.ptr),
            hdr.package_version_string.length);
        info("Package version: {VERSION}", "VERSION", this->pkgVersion);
    }
    this->componentBitmapBitLength = hdr.component_bitmap_bit_length;

    parseFDIdentificationArea();
    parseCompImageInfoArea();
    validatePkgTotalSize(pkgSize);
}

} // namespace fw_update

} // namespace pldm
