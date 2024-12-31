#pragma once

#include "common/utils.hpp"
#include "oem_meta_file_io_by_type.hpp"
#include "requester/configuration_discovery_handler.hpp"

namespace pldm::responder::oem_meta
{
/** @class CrashDumpHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to handle incoming crash dump requests from Hosts
 */
class CrashDumpHandler : public FileHandler
{
  public:
    CrashDumpHandler() = delete;
    CrashDumpHandler(const CrashDumpHandler&) = delete;
    CrashDumpHandler(CrashDumpHandler&&) = delete;
    CrashDumpHandler& operator=(const CrashDumpHandler&) = delete;
    CrashDumpHandler& operator=(CrashDumpHandler&&) = delete;
    ~CrashDumpHandler() = default;

    explicit CrashDumpHandler(
        uint8_t tid, std::map<std::string, MctpEndpoint>& configurations) :
        tid(tid),
        configurations(configurations)
    {}

    int write(const message& data) override;
    int read(struct pldm_oem_meta_file_io_read_resp* data) override
    {
        (void)data;
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

  private:
    /**
     * @brief Handles the control bank data and processes it into a
     * stringstream.
     *
     * @param data A span of constant uint8_t representing the control bank
     * data.
     * @param ss A stringstream to store the processed data.
     * @return int Status code indicating success or failure of the operation.
     */
    int handleCtrlBank(std::span<const uint8_t> data, std::stringstream& ss);

    /** @brief Get slot number from the configuration
     *  @return slot number in string
     */
    std::string getSlotNumberString();
    /** @brief The requester's TID */
    uint8_t tid = 0;

    /** @brief Configurations searched by ConfigurationDiscoveryHandler */
    std::map<std::string /*configDbusPath*/, MctpEndpoint>& configurations;
};

} // namespace pldm::responder::oem_meta
