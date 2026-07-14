#pragma once

#include "common/types.hpp"

#include <libpldm/firmware_update.h>

#include <libpldm++/firmware_update.hpp>

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
    explicit WrapPackageParser() {}
    WrapPackageParser(const WrapPackageParser&) = delete;
    WrapPackageParser(WrapPackageParser&&) = default;
    WrapPackageParser& operator=(const WrapPackageParser&) = delete;
    WrapPackageParser& operator=(WrapPackageParser&&) = delete;
    ~WrapPackageParser() = default;

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

  private:
    /** @brief Firmware Device ID Records in the package */
    FirmwareDeviceIDRecords fwDeviceIDRecords;

    /** @brief Component Image Information in the package */
    ComponentImageInfos componentImageInfos;

    // populated by parse(...)
    std::unique_ptr<Package> package;
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
