#pragma once

#include "common/utils.hpp"
#include "libpldmresponder/fru.hpp"
#include "libpldmresponder/oem_handler.hpp"

namespace pldm
{
namespace responder
{
using ObjectPath = std::string;
using AssociatedEntityMap = std::map<ObjectPath, pldm_entity>;

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

    pldm_entity getEntityIDfromRSI(uint16_t fruRSI);

    virtual const AssociatedEntityMap& getAssociateEntityMap()
    {
        return fruHandler->getAssociateEntityMap();
    }

    void updateDBusProperty(uint16_t fruRSI,
                            const AssociatedEntityMap& fruAssociationMap,
                            std::string vendorId, std::string deviceId,
                            std::string revisionId, std::string classCode,
                            std::string subSystemVendorId,
                            std::string subSystemId);

    ~Handler() = default;

    pldm::responder::fru::Handler* fruHandler; //!< pointer to PLDM fru handler

    /** @brief pointer to BMC's primary PDR repo */
    const pldm_pdr* pdrRepo;

    /** @brief variable to store the fru record set pdr entity*/
    pldm_entity fru_record_set_entity;
};
} // namespace oem_ibm_fru
} // namespace responder
} // namespace pldm
