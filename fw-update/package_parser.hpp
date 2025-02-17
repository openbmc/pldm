#pragma once

#include "common/types.hpp"

#include <libpldm/firmware_update.h>

#include <array>
#include <cstdint>
#include <memory>
#include <tuple>
#include <vector>

namespace pldm
{

namespace fw_update
{

/** @class PackageParser
 *
 *  PackageParser is the abstract base class for parsing the PLDM firmware
 *  update package. The PLDM firmware update contains two major sections; the
 *  firmware package header, and the firmware package payload. Each package
 *  header version will have a concrete implementation of the PackageParser.
 *  The concrete implementation understands the format of the package header and
 *  will implement the parse API.
 */
class PackageParser
{
  public:
    PackageParser() : pkgHeaderSize(0), componentBitmapBitLength(0), pkgIter{}
    {}

    PackageParser(const PackageParser&) = delete;
    PackageParser& operator=(const PackageParser&) = delete;

    PackageParser(PackageParser&&) = default;
    PackageParser& operator=(PackageParser&&) = default;

    virtual ~PackageParser() = default;

    /** @brief Parse the firmware update package header
     *
     *  @param[in] pkgHdr - Package header
     *  @param[in] pkgSize - Size of the firmware update package
     *
     *  @throws InternalFailure if parsing fails
     */
    virtual void parse(const std::vector<uint8_t>& pkgHdr,
                       uintmax_t pkgSize) = 0;

    /** @brief Get firmware device ID records from the package
     *
     *  @return Firmware device ID records if parsing the package is successful
     */
    const FirmwareDeviceIDRecords& getFwDeviceIDRecords() const
    {
        return fwDeviceIDRecords;
    }

    /** @brief Get component image information from the package
     *
     *  @return Component image information if parsing the package is successful
     */
    const ComponentImageInfos& getComponentImageInfos() const
    {
        return componentImageInfos;
    }

    /** @brief Size of the package header */
    PackageHeaderSize pkgHeaderSize = 0;

    /** @brief Package version string */
    PackageVersion pkgVersion{};

  protected:
    /** @brief Parse the firmware device identification area
     *
     *  @throws InternalFailure if parsing fails
     */
    void parseFDIdentificationArea();

    /** @brief Parse the component image information area
     *
     *  @throws InternalFailure if parsing fails
     */
    void parseCompImageInfoArea();

    /** @brief Validate the total size of the package
     *
     *  Verify the total size of the package is the sum of package header and
     *  the size of each component.
     *
     *  @param[in] pkgSize - firmware update package size
     *
     *  @throws InternalFailure if validation fails
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
    ComponentBitmapBitLength componentBitmapBitLength = 0;

    /** @brief Package iterator for parsing the firmware update package */
    struct pldm_firmware_update_package_iter pkgIter;
};

/** @class PackageParserV1
 *
 *  This class implements the package parser for the header format version 0x01
 */
class PackageParserV1 final : public PackageParser
{
  public:
    PackageParserV1() : PackageParser() {}

    PackageParserV1(const PackageParserV1&) = delete;
    PackageParserV1& operator=(const PackageParserV1&) = delete;

    PackageParserV1(PackageParserV1&&) = default;
    PackageParserV1& operator=(PackageParserV1&&) = default;

    ~PackageParserV1() = default;

    void parse(const std::vector<uint8_t>& pkgHdr, uintmax_t pkgSize) override;
};

} // namespace fw_update

} // namespace pldm
