#pragma once

#include "libpldmresponder/fru.hpp"
#include "libpldmresponder/oem_handler.hpp"

namespace pldm
{
namespace responder
{
namespace oem_ibm_fru
{

class Handler : public oem_fru::Handler
{
  public:
    Handler(const pldm::utils::DBusHandler* dBusIntf) :
        oem_fru::Handler(dBusIntf), fruHandler(nullptr)
    {}

    /** @brief Method to set the fru handler in the
     *    oem_ibm_handler class
     *
     *  @param[in] handler - pointer to PLDM platform handler
     */
    void setFruHandler(pldm::responder::fru::Handler* handler);

    /** @brief Process OEM FRU record
     *
     *  @param[in] fruData - the data of the fru
     *  @param[in] updateDBus - update DBus property if this is set to true
     *
     */
    void processOEMfruRecord(const std::vector<uint8_t>& fruData,
                             bool updateDBus = true);

    ~Handler() = default;

    pldm::responder::fru::Handler* fruHandler; //!< pointer to PLDM fru handler
};
} // namespace oem_ibm_fru
} // namespace responder
} // namespace pldm
