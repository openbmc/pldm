#include "libpldm/firmware_update.h"

#include "common/types.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <tuple>
#include <vector>
namespace pldm
{

namespace fw_update
{

class PackageParser
{
  public:
    PackageParser() = delete;
    PackageParser(const PackageParser&) = delete;
    PackageParser(PackageParser&&) = default;
    PackageParser& operator=(const PackageParser&) = delete;
    PackageParser& operator=(PackageParser&&) = default;
    virtual ~PackageParser() = default;

    explicit PackageParser(size_t pkgHeaderSize, std::string pkgVersion,
                           uint16_t componentBitmapBitLength) :
        pkgHeaderSize(pkgHeaderSize),
        pkgVersion(pkgVersion),
        componentBitmapBitLength(componentBitmapBitLength)
    {}

    virtual void parse(const std::vector<uint8_t>& pkgHdr,
                       uintmax_t pkgSize) = 0;

    const FirmwareDeviceIDRecords& getFwDeviceIDRecords()
    {
        return fwDeviceIDRecords;
    }

    const ComponentImageInfos& getComponentImageInfos()
    {
        return componentImageInfos;
    }

    const size_t pkgHeaderSize;
    const std::string pkgVersion;

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

    void validatePkgTotalSize(uintmax_t pkgSize);

    FirmwareDeviceIDRecords fwDeviceIDRecords;
    ComponentImageInfos componentImageInfos;
    const uint16_t componentBitmapBitLength;
};

class PackageParserV1 final : public PackageParser
{
  public:
    PackageParserV1() = delete;
    PackageParserV1(const PackageParserV1&) = delete;
    PackageParserV1(PackageParserV1&&) = default;
    PackageParserV1& operator=(const PackageParserV1&) = delete;
    PackageParserV1& operator=(PackageParserV1&&) = default;
    ~PackageParserV1() = default;

    explicit PackageParserV1(size_t pkgHeaderSize, std::string pkgVersion,
                             uint16_t componentBitmapBitLength) :
        PackageParser(pkgHeaderSize, pkgVersion, componentBitmapBitLength)
    {}

    virtual void parse(const std::vector<uint8_t>& pkgHdr, uintmax_t pkgSize);
};

/** @brief Parse the package header information
 *
 *  @param[in] pkgHdrInfo - component image count
 *  @param[in] pkgHdr - firmware package header
 *  @param[in] offset - offset in package header which is the start of the
 *                      component image information area
 *
 *  @return On success return the offset which is the end of the component
 *          image information area, on error throw exception.
 */
std::unique_ptr<PackageParser> parsePkgHeader(std::vector<uint8_t>& pkgHdrInfo);

} // namespace fw_update

} // namespace pldm
