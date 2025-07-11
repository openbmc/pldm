#pragma once

#include "common/utils.hpp"
#include "file_io_by_type.hpp"
#include "oem/meta/requester/configuration_discovery_handler.hpp"

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
        uint8_t tid,
        std::map<std::string, pldm::oem_meta::MctpEndpoint>& configurations) :
        tid(tid), configurations(configurations)
    {}

    /** @brief Method to send crashdump data to BIC, BIC will send the
     * 				 data to the BMC. The BMC handler need to decode the data
     * 				 and generate the crashdump file.
     *  @param[in] data - crashdump raw data.
     *  @return  PLDM completion code.
     */
    int write(const message& data) override;

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
    std::map<std::string /*configDbusPath*/, pldm::oem_meta::MctpEndpoint>&
        configurations;
};

} // namespace pldm::responder::oem_meta
