#pragma once

#include <libpldm/base.h>
#include <libpldm/oem/meta/file_io.h>

#include <cstdint>
#include <span>

namespace pldm::responder::oem_meta
{
using message = std::span<uint8_t>;

enum class FileIOType : uint8_t
{
    POST_CODE = 0x00,
    BIOS_VERSION = 0x01,
    POWER_CONTROL = 0x02,
    HTTP_BOOT = 0x03,
    APML_ALERT = 0x04,
    EVENT_LOG = 0x05,
    CRASH_DUMP = 0x06
};

/**
 *  @class FileHandler
 *
 *  Base class to handle read/write of all OEM file types
 */
class FileHandler
{
  protected:
    FileHandler() = default;

  public:
    FileHandler(const FileHandler&) = delete;
    FileHandler(FileHandler&&) = delete;
    FileHandler& operator=(const FileHandler&) = delete;
    FileHandler& operator=(FileHandler&&) = delete;
    virtual ~FileHandler() = default;

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
};

} // namespace pldm::responder::oem_meta
