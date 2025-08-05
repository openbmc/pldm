#pragma once

#include "common/utils.hpp"
#include "file_io_by_type.hpp"
#include "oem/meta/utils.hpp"

#define BOOT_MODE_UEFI 0x01
#define BOOT_MODE_CMOS_CLR 0x02
#define BOOT_MODE_FORCE_BOOT 0x04
#define BOOT_MODE_BOOT_FLAG 0x80

namespace pldm::responder::oem_meta
{

/** @class BootOrderHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to store boot order
 */
class BootOrderHandler : public FileHandler
{
  public:
    explicit BootOrderHandler(
        pldm_tid_t tid,
        const std::map<eid, pldm::oem_meta::MctpEndpoint>& configurations,
        const pldm::utils::DBusHandler* dBusIntf) :
        tid(tid), configurations(configurations), dBusIntf(dBusIntf)
    {}

    /** @brief Method to get/set boot order.
     *  @param[in] data - boot order raw data.
     *  @return  PLDM completion code.
     */
    int write(const message& data) override;
    int read(struct pldm_oem_meta_file_io_read_resp* data) override;

  private:
    /** @brief The requester's TID */
    pldm_tid_t tid = 0;

    /** @brief Configurations searched by ConfigurationDiscoveryHandler */
    const std::map<eid, pldm::oem_meta::MctpEndpoint>& configurations;

    /** @brief D-Bus Interface object*/
    const pldm::utils::DBusHandler* dBusIntf;
};

} // namespace pldm::responder::oem_meta
