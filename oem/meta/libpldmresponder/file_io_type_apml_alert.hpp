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

    /** @brief Method to call `amd-ras` to do ADDC
     *  @param[in] data - APML alert raw data.
     *  @return  PLDM completion code.
     */
    int write(const message& data) override;

  private:
    /** @brief The requester's TID */
    uint8_t tid = 0;
};

} // namespace pldm::responder::oem_meta
