#pragma once

#include "common/utils.hpp"
#include "oem_meta_file_io_by_type.hpp"

namespace pldm::responder::oem_meta
{
/** @class SledCycleHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to handle incoming sled cycle request from Hosts
 */
class SledCycleHandler : public FileHandler
{
  public:
    SledCycleHandler() = delete;
    SledCycleHandler(const SledCycleHandler&) = delete;
    SledCycleHandler(SledCycleHandler&&) = delete;
    SledCycleHandler& operator=(const SledCycleHandler&) = delete;
    SledCycleHandler& operator=(SledCycleHandler&&) = delete;
    ~SledCycleHandler() = default;

    explicit SledCycleHandler(uint8_t tid,
                              const pldm::utils::DBusHandler* dBusIntf) :
        tid(tid),
        dBusIntf(dBusIntf)
    {}

    int write(const message& data) override;
    int read(const message&) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

  private:
    /** @brief The requester's TID */
    uint8_t tid = 0;

    /** @brief D-Bus Interface object*/
    const pldm::utils::DBusHandler* dBusIntf;
};

} // namespace pldm::responder::oem_meta
