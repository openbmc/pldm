#include "package_parser.hpp"

#include "common/utils.hpp"

#include <libpldm/edac.h>
#include <libpldm/firmware_update.h>

#include <libpldm++/firmware_update.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <memory>
#include <utility>

PHOSPHOR_LOG2_USING;

namespace pldm
{

namespace fw_update
{

using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

/** @brief Checks which the deprecated decode_pldm_comp_image_info() enforced
 *         but the libpldm package iterators do not.
 *
 *  @note Throws InternalFailure if the component image information is not
 *        usable for an update.
 */
static void validateComponentImageInfo(const ComponentImageInfo& c)
{
    if (c.componentLocation.ptr == nullptr || c.componentLocation.length == 0)
    {
        error(
            "Component image is empty for classification '{CLASSIFICATION}' and identifier '{IDENTIFIER}'",
            "CLASSIFICATION", c.componentClassification, "IDENTIFIER",
            c.componentIdentifier);
        throw InternalFailure();
    }

    // An empty ComponentVersionString is rejected by libpldm++ itself, so it is
    // not checked here.

    // DSP0267: when the UseComponentComparisonStamp bit is not set, the
    // ComponentComparisonStamp shall be 0xFFFFFFFF.
    constexpr uint32_t unusedComparisonStamp = 0xFFFFFFFF;
    constexpr size_t useComparisonStampBit = 1;
    if (!c.componentOptions.test(useComparisonStampBit) &&
        c.compComparisonStamp != unusedComparisonStamp)
    {
        error(
            "Component comparison stamp '{STAMP}' has to be 0xFFFFFFFF while UseComponentComparisonStamp is not set, for identifier '{IDENTIFIER}'",
            "STAMP", c.compComparisonStamp, "IDENTIFIER",
            c.componentIdentifier);
        throw InternalFailure();
    }
}

static WrapComponentImageInfo convertComponentImageInfo(
    const ComponentImageInfo& c)
{
    validateComponentImageInfo(c);

    return {c.componentClassification,
            c.componentIdentifier,
            c.compComparisonStamp,
            c.componentOptions,
            c.requestedComponentActivationMethod,
            CompImage{c.componentLocation.ptr, c.componentLocation.length},
            c.componentVersion};
}

static std::variant<WrapDescriptorData, VendorDefinedDescriptorInfo>
    convertDescriptor(const DescriptorData& dd)
{
    if (dd.vendorDefinedDescriptorTitle.has_value())
    {
        return VendorDefinedDescriptorInfo{
            dd.vendorDefinedDescriptorTitle.value(), dd.data};
    }
    return dd.data;
}

static WrapFirmwareDeviceIDRecord convertFirmwareDeviceIDRecord(
    const FirmwareDeviceIDRecord& f)
{
    Descriptors recordDescriptors;
    for (const auto& [k, v] : f.recordDescriptors)
    {
        recordDescriptors.insert({k, convertDescriptor(*v)});
    }

    return {f.deviceUpdateOptionFlags, f.applicableComponents,
            f.componentImageSetVersionString, recordDescriptors,
            f.firmwareDevicePackageData};
}

const static PackagePin currentPin = PackagePin::v1;

WrapPackageParser::WrapPackageParser(std::span<const uint8_t> pkgHdr)
{
    auto expected = pldm::fw_update::PackageParser::parse(pkgHdr, currentPin);

    if (!expected.has_value())
    {
        error("Failed to parse the fw update package: {ERR}, RC = {RC}", "ERR",
              expected.error().msg, "RC", expected.error().rc.value_or(0));
        throw InternalFailure();
    }

    // Only used to populate the members below. The component image views point
    // into pkgHdr, not into the package, so it does not have to be kept alive.
    const std::unique_ptr<Package> package = std::move(expected.value());

    for (const ComponentImageInfo& cii : package->componentImageInformation)
    {
        componentImageInfos.emplace_back(convertComponentImageInfo(cii));
    }

    for (const FirmwareDeviceIDRecord& fdir : package->firmwareDeviceIdRecords)
    {
        fwDeviceIDRecords.emplace_back(convertFirmwareDeviceIDRecord(fdir));
    }
}

std::unique_ptr<WrapPackageParser> parsePkgHeader(
    std::span<const uint8_t> pkgHdr)
{
    try
    {
        return std::make_unique<WrapPackageParser>(pkgHdr);
    }
    catch (const InternalFailure&)
    {
        return nullptr;
    }
}

} // namespace fw_update

} // namespace pldm
