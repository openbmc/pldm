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
    PackageParser() = delete;
    PackageParser(const PackageParser&) = delete;
    PackageParser(PackageParser&&) = default;
    PackageParser& operator=(const PackageParser&) = delete;
    PackageParser& operator=(PackageParser&&) = delete;
    virtual ~PackageParser() = default;

    /** @brief Constructor
     *
     *  @param[in] pkgData - Package data
     */
    explicit PackageParser(std::vector<uint8_t>& pkgData)
    {
        parse(pkgData);
    }

    /** @brief Get firmware device ID records from the package
     *
     *  @return if parsing the package is successful, return firmware device ID
     *          records
     */
    const FirmwareDeviceIDRecords& getFwDeviceIDRecords() const
    {
        return fwDeviceIDRecords;
    }

    /** @brief Get downstream device ID records from the package
     *
     *  @return if parsing the package is successful, return downstream device
     * ID records
     */
    const DownstreamDeviceIDRecords& getDownstreamDeviceIDRecords() const
    {
        return downstreamDeviceIDRecords;
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

    /** @brief Package version string */
    PackageVersion pkgVersion;

  protected:
    /** @brief Parse the firmware update package header
     *
     *  @param[in] pkgHdr - Package header
     *
     *  @note Throws exception if parsing fails
     */
    virtual void parse(const std::vector<uint8_t>& pkgHdr);
    /** @brief Firmware Device ID Records in the package */
    FirmwareDeviceIDRecords fwDeviceIDRecords;

    /** @brief Downstream Device ID Records in the package */
    DownstreamDeviceIDRecords downstreamDeviceIDRecords;

    /** @brief Component Image Information in the package */
    ComponentImageInfos componentImageInfos;

    /** @brief The number of bits that will be used to represent the bitmap in
     *         the ApplicableComponents field for matching device. The value
     *         shall be a multiple of 8 and be large enough to contain a bit
     *         for each component in the package.
     */
    ComponentBitmapBitLength componentBitmapBitLength;
};

} // namespace fw_update

} // namespace pldm
