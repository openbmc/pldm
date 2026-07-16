#pragma once

#include "common/types.hpp"

#include <libpldm/firmware_update.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace pldm
{

namespace fw_update
{

/** @class PackageParser
 *
 *  PackageParser is the class for parsing the PLDM firmware update package.
 */
class WrapPackageParser
{
  public:
    WrapPackageParser() = delete;
    WrapPackageParser(const WrapPackageParser&) = delete;
    WrapPackageParser(WrapPackageParser&&) = default;
    WrapPackageParser& operator=(const WrapPackageParser&) = delete;
    WrapPackageParser& operator=(WrapPackageParser&&) = delete;
    ~WrapPackageParser() = default;

    /** @brief Constructor
     *
     *  @param[in] pkgHeaderSize - Size of package header section
     *  @param[in] pkgVersion - Package version
     *  @param[in] componentBitmapBitLength - The number of bits used to
     *                                        represent the bitmap in the
     *                                        ApplicableComponents field for a
     *                                        matching device.
     */
    explicit WrapPackageParser(
        PackageHeaderSize pkgHeaderSize, const PackageVersion& pkgVersion,
        ComponentBitmapBitLength componentBitmapBitLength) :
        pkgHeaderSize(pkgHeaderSize), pkgVersion(pkgVersion),
        componentBitmapBitLength(componentBitmapBitLength)
    {}

    /** @brief Parse the firmware update package
     *
     *  @param[in] pkgHdr - Package
     *
     *  @note Throws exception is parsing fails
     */
    void parse(const std::vector<uint8_t>& pkgHdr);

    /** @brief Get firmware device ID records from the package
     *
     *  @return if parsing the package is successful, return firmware device ID
     *          records
     */
    const FirmwareDeviceIDRecords& getFwDeviceIDRecords() const
    {
        return fwDeviceIDRecords;
    }

    /** @brief Get component image information from the package
     *
     *  @return if parsing the package is successful, return component image
     *          information
     */
    const ComponentImageInfos& getComponentImageInfos() const
    {
        return componentImageInfos;
    }

    /** @brief Device identifiers of the managed FDs */
    const PackageHeaderSize pkgHeaderSize;

    /** @brief Package version string */
    const PackageVersion pkgVersion;

  protected:
    /** @brief Parse the firmware device identification area
     *
     *  @param[in] deviceIdRecCount - count of firmware device ID records
     *  @param[in] pkgHdr - firmware package header
     *  @param[in] offset - offset in package header which is the start of the
     *                      firmware device identification area
     *
     *  @return On success return the offset which is the end of the firmware
     *          device identification area, on error throw exception.
     */
    size_t parseFDIdentificationArea(DeviceIDRecordCount deviceIdRecCount,
                                     const std::vector<uint8_t>& pkgHdr,
                                     size_t offset);

    /** @brief Parse the component image information area
     *
     *  @param[in] compImageCount - component image count
     *  @param[in] pkgHdr - firmware package header
     *  @param[in] offset - offset in package header which is the start of the
     *                      component image information area
     *
     *  @return On success return the offset which is the end of the component
     *          image information area, on error throw exception.
     */
    size_t parseCompImageInfoArea(ComponentImageCount compImageCount,
                                  const std::vector<uint8_t>& pkgHdr,
                                  size_t offset);

    /** @brief Validate the total size of the package
     *
     *  Verify the total size of the package is the sum of package header and
     *  the size of each component.
     *
     *  @param[in] pkgSize - firmware update package size
     *
     *  @note Throws exception if validation fails
     */
    void validatePkgTotalSize(uintmax_t pkgSize);

    /** @brief Firmware Device ID Records in the package */
    FirmwareDeviceIDRecords fwDeviceIDRecords;

    /** @brief Component Image Information in the package */
    ComponentImageInfos componentImageInfos;

    /** @brief The number of bits that will be used to represent the bitmap in
     *         the ApplicableComponents field for matching device. The value
     *         shall be a multiple of 8 and be large enough to contain a bit
     *         for each component in the package.
     */
    const ComponentBitmapBitLength componentBitmapBitLength;
};

/** @brief Parse the package header information
 *
 *  @param[in] pkgHdrInfo - package header information section in the package
 *
 *  @return On success return the PackageParser for the header format version
 *          on failure return nullptr
 */
std::unique_ptr<WrapPackageParser> parsePkgHeader(
    std::vector<uint8_t>& pkgHdrInfo);

} // namespace fw_update

} // namespace pldm
