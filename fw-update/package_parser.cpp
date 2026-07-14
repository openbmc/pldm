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

static WrapComponentImageInfo convertComponentImageInfo(
    const ComponentImageInfo& c, const uint8_t* pkgPtr)
{
    return {c.componentClassification,
            c.componentIdentifier,
            c.compComparisonStamp,
            c.componentOptions,
            c.requestedComponentActivationMethod,
            c.componentLocation.ptr - pkgPtr,
            c.componentLocation.length,
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

void WrapPackageParser::parse(const std::vector<uint8_t>& pkgHdr)
{
    auto expected = pldm::fw_update::PackageParser::parse(pkgHdr, currentPin);

    if (expected.has_value())
    {
        package = std::move(expected.value());
    }
    else
    {
        error("{ERR}", "ERR", expected.error().msg);
        throw InternalFailure();
    }

    // populate member variables

    const uint8_t* pkgPtr = pkgHdr.data();

    for (const ComponentImageInfo& cii : package->componentImageInformation)
    {
        componentImageInfos.emplace_back(
            convertComponentImageInfo(cii, pkgPtr));
    }

    for (const FirmwareDeviceIDRecord& fdir : package->firmwareDeviceIdRecords)
    {
        fwDeviceIDRecords.emplace_back(convertFirmwareDeviceIDRecord(fdir));
    }
}

std::unique_ptr<WrapPackageParser> parsePkgHeader(std::vector<uint8_t>& pkgHdr)
{
    auto expected = pldm::fw_update::PackageParser::parse(pkgHdr, currentPin);

    if (!expected.has_value())
    {
        error("{ERR}, RC = {RC}", "ERR", expected.error().msg, "RC",
              expected.error().rc.value_or(0));
        return nullptr;
    }

    return std::make_unique<WrapPackageParser>();
}

} // namespace fw_update

} // namespace pldm
