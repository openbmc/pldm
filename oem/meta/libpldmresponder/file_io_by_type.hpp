#pragma once
#include <libpldm/base.h>
#include <libpldm/oem/meta/file_io.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pldm::responder::oem_meta
{
/**
 *  @class FileHandler
 *
 *  Base class to handle read/write of all OEM file types
 */

using message = std::span<uint8_t>;
using configDbusPath = std::string;
constexpr auto decodeDataMaxLength = 240;

enum class FileIOType : uint8_t
{
    POST_CODE = 0x00,
    BIOS_VERSION = 0x01,
    POWER_CONTROL = 0x02,
    HTTP_BOOT = 0x03
};

class FileHandler
{
  public:
    /** @brief Method to write data to BMC
     *  @param[in] data - eventData
     *  @return  PLDM status code
     */
    virtual int write([[maybe_unused]] const message& data)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    /** @brief Method to read data from BIC
     *  @param[in] data - eventData
     *  @return  PLDM status code
     */
    virtual int read(
        [[maybe_unused]] struct pldm_oem_meta_file_io_read_resp* data)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }
    virtual ~FileHandler() {}
};

} // namespace pldm::responder::oem_meta
