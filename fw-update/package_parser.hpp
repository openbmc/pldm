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

/** @class WrapPackageParser
 *
 *  WrapPackageParser parses the PLDM firmware update package via libpldm++ and
 *  presents the result using the data structures of this repository.
 *
 *  @note The component images are exposed as views into @a pkgHdr, so the
 *        buffer the package was parsed from has to outlive this object.
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

    /** @brief Constructor, parses the firmware update package
     *
     *  @param[in] pkgHdr - the whole firmware update package
     *
     *  @note Throws InternalFailure if parsing fails
     */
    explicit WrapPackageParser(std::span<const uint8_t> pkgHdr);

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
};

/** @brief Parse the firmware update package
 *
 *  @param[in] pkgHdr - the whole firmware update package
 *
 *  @return On success return the WrapPackageParser, on failure return nullptr
 */
std::unique_ptr<WrapPackageParser> parsePkgHeader(
    std::span<const uint8_t> pkgHdr);

} // namespace fw_update

} // namespace pldm
