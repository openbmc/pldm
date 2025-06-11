#pragma once

#include "common/utils.hpp"
#include "file_io_by_type.hpp"

namespace pldm::responder::oem_meta
{
/** @class APMLAlertHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to handle incoming sled cycle request from Hosts
 */
class APMLAlertHandler : public FileHandler
{
  public:
    APMLAlertHandler() = delete;
    APMLAlertHandler(const APMLAlertHandler&) = delete;
    APMLAlertHandler(APMLAlertHandler&&) = delete;
    APMLAlertHandler& operator=(const APMLAlertHandler&) = delete;
    APMLAlertHandler& operator=(APMLAlertHandler&&) = delete;
    ~APMLAlertHandler() = default;

    explicit APMLAlertHandler(uint8_t tid) : tid(tid) {}

    int write(const message& data) override;
    int read(struct pldm_oem_meta_file_io_read_resp* data) override
    {
        (void)data; // Unused
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

  private:
    /** @brief The requester's TID */
    uint8_t tid = 0;
};

} // namespace pldm::responder::oem_meta
