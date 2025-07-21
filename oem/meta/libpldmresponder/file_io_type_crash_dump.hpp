#pragma once

#include "common/utils.hpp"
#include "file_io_by_type.hpp"
#include "oem/meta/utils.hpp"

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

    explicit CrashDumpHandler(
        pldm_tid_t tid,
        std::map<pldm::dbus::ObjectPath, pldm::oem_meta::MctpEndpoint>&
            configurations) : tid(tid), configurations(configurations)
    {}

    ~CrashDumpHandler() = default;

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

    /**
     * @brief Handles the CXL crashdump to log file.
     *
     * @param data A span of constant uint8_t representing the CXL crashdump
     * @param ss A stringstream to store the processed data.
     * @return int Status code indicating success or failure of the operation.
     */
    int handleCxlCrashdump(std::span<const uint8_t> data);

    /** @brief The requester's TID */
    pldm_tid_t tid = 0;

    /** @brief Configurations searched by ConfigurationDiscoveryHandler */
    std::map<pldm::dbus::ObjectPath /*configDbusPath*/,
             pldm::oem_meta::MctpEndpoint>& configurations;
};

} // namespace pldm::responder::oem_meta
