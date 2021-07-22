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

struct PcieConfigSpaceData
{
    uint16_t vendorId;
    uint16_t deviceId;
    uint32_t first_reserved;
    uint8_t revisionId;
    std::array<uint8_t, 3> classCode;
    uint32_t second_reserved[8];
    uint16_t subSystemVendorId;
    uint16_t subSystemId;
    uint32_t lastZeros[4];

} __attribute__((packed));

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

    /** @brief Process OEM FRU record
     *
     *  @param[in] fruData - the data of the fru
     *  @param[in] updateDBus - update DBus property if this is set to true
     *
     */
    void processOEMfruRecord(const std::vector<uint8_t>& fruData,
                             bool updateDBus = true);

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

    /** @brief pointer to BMC's primary PDR repo */
    const pldm_pdr* pdrRepo;

    pldm::responder::fru::Handler* fruHandler; //!< pointer to PLDM fru handler
};
} // namespace oem_ibm_fru
} // namespace responder
} // namespace pldm
