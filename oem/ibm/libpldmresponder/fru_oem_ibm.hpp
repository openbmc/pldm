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
    Handler(const pldm::utils::DBusHandler* dBusIntf, pldm_pdr* repo) :
        oem_fru::Handler(dBusIntf), pdrRepo(repo)
    {}

    /** @brief Method to set the fru handler in the
     *    oem_ibm_handler class
     *
     *  @param[in] handler - pointer to PLDM platform handler
     */
    void setFruHandler(pldm::responder::fru::Handler* handler);

  private:
    /** @brief pointer to BMC's primary PDR repo */
    const pldm_pdr* pdrRepo;

    pldm::responder::fru::Handler* fruHandler; //!< pointer to PLDM fru handler
};

} // namespace oem_ibm_fru
} // namespace responder
} // namespace pldm
