#pragma once

#include "common/types.hpp"

#include <libpldm/firmware_update.h>

#include <libpldm++/firmware_update.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace pldm
{

namespace fw_update
{

namespace pkg
{

/** @class PackageParser
 *
 *  PackageParser parses the PLDM firmware update package via libpldm++ and
 *  presents the result using the data structures of this repository.
 *
 *  @note The component images are exposed as views into @a pkgHdr, so the
 *        buffer the package was parsed from has to outlive this object.
 */
class PackageParser
{
  public:
    explicit PackageParser() {}
    PackageParser(const PackageParser&) = delete;
    PackageParser(PackageParser&&) = default;
    PackageParser& operator=(const PackageParser&) = delete;
    PackageParser& operator=(PackageParser&&) = delete;
    ~PackageParser() = default;

    /** @brief Get firmware device ID records from the package
     *
     *  @return if parsing the package is successful, return firmware device ID
     *          records
     */
    const pkg::FirmwareDeviceIDRecords& getFwDeviceIDRecords() const
    {
        return fwDeviceIDRecords;
    }

    /** @brief Get downstream device ID records from the package
     *
     *  @return if parsing the package is successful, return downstream device
     *          ID records. Empty for packages older than v1.1.0.
     */
    const pkg::DownstreamDeviceIDRecords& getDownstreamDeviceIDRecords() const
    {
        return downstreamDeviceIDRecords;
    }

    /** @brief Get component image information from the package
     *
     *  @return if parsing the package is successful, return component image
     *          information
     */
    const pkg::ComponentImageInfos& getComponentImageInfos() const
    {
        return componentImageInfos;
    }

    /** @brief Parse the package header information
     *
     *  @param[in] pkgHdr - the whole firmware update package
     *
     *  @return On success return the PackageParser for the header format
     * version, on failure throws exception
     */
    static std::unique_ptr<PackageParser> parsePkgHeader(
        std::span<const uint8_t> pkgHdr);

  private:
    /** @brief Parse the firmware update package
     *
     *  @param[in] pkgHdr - Package
     *
     *  @note Throws exception is parsing fails
     */
    void parse(std::span<const uint8_t> pkgHdr);

    /** @brief Firmware Device ID Records in the package */
    pkg::FirmwareDeviceIDRecords fwDeviceIDRecords;

    /** @brief Downstream Device ID Records in the package */
    pkg::DownstreamDeviceIDRecords downstreamDeviceIDRecords;

    /** @brief Component Image Information in the package */
    pkg::ComponentImageInfos componentImageInfos;
};

} // namespace pkg

} // namespace fw_update

} // namespace pldm
